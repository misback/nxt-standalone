// Copyright 2017 The NXT Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "backend/BindGroup.h"

#include "backend/BindGroupLayout.h"
#include "backend/Buffer.h"
#include "backend/Device.h"
#include "backend/Texture.h"
#include "common/Assert.h"
#include "common/Math.h"

namespace backend {

    // BindGroup

    BindGroupBase::BindGroupBase(BindGroupBuilder* builder)
        : mLayout(std::move(builder->mLayout)),
          mUsage(builder->mUsage),
          mBindings(std::move(builder->mBindings)) {
    }

    const BindGroupLayoutBase* BindGroupBase::GetLayout() const {
        return mLayout.Get();
    }

    nxt::BindGroupUsage BindGroupBase::GetUsage() const {
        return mUsage;
    }

    BufferViewBase* BindGroupBase::GetBindingAsBufferView(size_t binding) {
        ASSERT(binding < kMaxBindingsPerGroup);
        ASSERT(mLayout->GetBindingInfo().mask[binding]);
        ASSERT(mLayout->GetBindingInfo().types[binding] == nxt::BindingType::UniformBuffer ||
               mLayout->GetBindingInfo().types[binding] == nxt::BindingType::StorageBuffer);
        return reinterpret_cast<BufferViewBase*>(mBindings[binding].Get());
    }

    SamplerBase* BindGroupBase::GetBindingAsSampler(size_t binding) {
        ASSERT(binding < kMaxBindingsPerGroup);
        ASSERT(mLayout->GetBindingInfo().mask[binding]);
        ASSERT(mLayout->GetBindingInfo().types[binding] == nxt::BindingType::Sampler);
        return reinterpret_cast<SamplerBase*>(mBindings[binding].Get());
    }

    TextureViewBase* BindGroupBase::GetBindingAsTextureView(size_t binding) {
        ASSERT(binding < kMaxBindingsPerGroup);
        ASSERT(mLayout->GetBindingInfo().mask[binding]);
        ASSERT(mLayout->GetBindingInfo().types[binding] == nxt::BindingType::SampledTexture);
        return reinterpret_cast<TextureViewBase*>(mBindings[binding].Get());
    }

    DeviceBase* BindGroupBase::GetDevice() const {
        return mLayout->GetDevice();
    }

    // BindGroupBuilder

    enum BindGroupSetProperties {
        BINDGROUP_PROPERTY_USAGE = 0x1,
        BINDGROUP_PROPERTY_LAYOUT = 0x2,
    };

    BindGroupBuilder::BindGroupBuilder(DeviceBase* device) : Builder(device) {
    }

    BindGroupBase* BindGroupBuilder::GetResultImpl() {
        constexpr int allProperties = BINDGROUP_PROPERTY_USAGE | BINDGROUP_PROPERTY_LAYOUT;
        if ((mPropertiesSet & allProperties) != allProperties) {
            HandleError("Bindgroup missing properties");
            return nullptr;
        }

        if (mSetMask != mLayout->GetBindingInfo().mask) {
            HandleError("Bindgroup missing bindings");
            return nullptr;
        }

        return mDevice->CreateBindGroup(this);
    }

    void BindGroupBuilder::SetLayout(BindGroupLayoutBase* layout) {
        if ((mPropertiesSet & BINDGROUP_PROPERTY_LAYOUT) != 0) {
            HandleError("Bindgroup layout property set multiple times");
            return;
        }

        mLayout = layout;
        mPropertiesSet |= BINDGROUP_PROPERTY_LAYOUT;
    }

    void BindGroupBuilder::SetUsage(nxt::BindGroupUsage usage) {
        if ((mPropertiesSet & BINDGROUP_PROPERTY_USAGE) != 0) {
            HandleError("Bindgroup usage property set multiple times");
            return;
        }

        mUsage = usage;
        mPropertiesSet |= BINDGROUP_PROPERTY_USAGE;
    }

    void BindGroupBuilder::SetBufferViews(uint32_t start,
                                          uint32_t count,
                                          BufferViewBase* const* bufferViews) {
        if (!SetBindingsValidationBase(start, count)) {
            return;
        }

        const auto& layoutInfo = mLayout->GetBindingInfo();
        for (size_t i = start, j = 0; i < start + count; ++i, ++j) {
            nxt::BufferUsageBit requiredBit = nxt::BufferUsageBit::None;
            switch (layoutInfo.types[i]) {
                case nxt::BindingType::UniformBuffer:
                    requiredBit = nxt::BufferUsageBit::Uniform;
                    break;

                case nxt::BindingType::StorageBuffer:
                    requiredBit = nxt::BufferUsageBit::Storage;
                    break;

                case nxt::BindingType::Sampler:
                case nxt::BindingType::SampledTexture:
                    HandleError("Setting buffer for a wrong binding type");
                    return;
            }

            if (!(bufferViews[j]->GetBuffer()->GetAllowedUsage() & requiredBit)) {
                HandleError("Buffer needs to allow the correct usage bit");
                return;
            }

            if (!IsAligned(bufferViews[j]->GetOffset(), 256)) {
                HandleError("Buffer view offset for bind group needs to be 256-byte aligned");
                return;
            }
        }

        SetBindingsBase(start, count, reinterpret_cast<RefCounted* const*>(bufferViews));
    }

    void BindGroupBuilder::SetSamplers(uint32_t start,
                                       uint32_t count,
                                       SamplerBase* const* samplers) {
        if (!SetBindingsValidationBase(start, count)) {
            return;
        }

        const auto& layoutInfo = mLayout->GetBindingInfo();
        for (size_t i = start, j = 0; i < start + count; ++i, ++j) {
            if (layoutInfo.types[i] != nxt::BindingType::Sampler) {
                HandleError("Setting binding for a wrong layout binding type");
                return;
            }
        }

        SetBindingsBase(start, count, reinterpret_cast<RefCounted* const*>(samplers));
    }

    void BindGroupBuilder::SetTextureViews(uint32_t start,
                                           uint32_t count,
                                           TextureViewBase* const* textureViews) {
        if (!SetBindingsValidationBase(start, count)) {
            return;
        }

        const auto& layoutInfo = mLayout->GetBindingInfo();
        for (size_t i = start, j = 0; i < start + count; ++i, ++j) {
            if (layoutInfo.types[i] != nxt::BindingType::SampledTexture) {
                HandleError("Setting binding for a wrong layout binding type");
                return;
            }

            if (!(textureViews[j]->GetTexture()->GetAllowedUsage() &
                  nxt::TextureUsageBit::Sampled)) {
                HandleError("Texture needs to allow the sampled usage bit");
                return;
            }
        }

        SetBindingsBase(start, count, reinterpret_cast<RefCounted* const*>(textureViews));
    }

    void BindGroupBuilder::SetBindingsBase(uint32_t start,
                                           uint32_t count,
                                           RefCounted* const* objects) {
        for (size_t i = start, j = 0; i < start + count; ++i, ++j) {
            mSetMask.set(i);
            mBindings[i] = objects[j];
        }
    }

    bool BindGroupBuilder::SetBindingsValidationBase(uint32_t start, uint32_t count) {
        if (start + count > kMaxBindingsPerGroup) {
            HandleError("Setting bindings type over maximum number of bindings");
            return false;
        }

        if ((mPropertiesSet & BINDGROUP_PROPERTY_LAYOUT) == 0) {
            HandleError("Bindgroup layout must be set before views");
            return false;
        }

        const auto& layoutInfo = mLayout->GetBindingInfo();
        for (size_t i = start, j = 0; i < start + count; ++i, ++j) {
            if (mSetMask[i]) {
                HandleError("Setting already set binding");
                return false;
            }

            if (!layoutInfo.mask[i]) {
                HandleError("Setting binding that isn't present in the layout");
                return false;
            }
        }

        return true;
    }
}  // namespace backend
