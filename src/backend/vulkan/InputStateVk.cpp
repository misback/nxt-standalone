// Copyright 2018 The NXT Authors
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

#include "backend/vulkan/InputStateVk.h"

#include "common/BitSetIterator.h"

namespace backend { namespace vulkan {

    namespace {

        VkVertexInputRate VulkanInputRate(nxt::InputStepMode stepMode) {
            switch (stepMode) {
                case nxt::InputStepMode::Vertex:
                    return VK_VERTEX_INPUT_RATE_VERTEX;
                case nxt::InputStepMode::Instance:
                    return VK_VERTEX_INPUT_RATE_INSTANCE;
                default:
                    UNREACHABLE();
            }
        }

        VkFormat VulkanVertexFormat(nxt::VertexFormat format) {
            switch (format) {
                case nxt::VertexFormat::FloatR32G32B32A32:
                    return VK_FORMAT_R32G32B32A32_SFLOAT;
                case nxt::VertexFormat::FloatR32G32B32:
                    return VK_FORMAT_R32G32B32_SFLOAT;
                case nxt::VertexFormat::FloatR32G32:
                    return VK_FORMAT_R32G32_SFLOAT;
                case nxt::VertexFormat::FloatR32:
                    return VK_FORMAT_R32_SFLOAT;
                case nxt::VertexFormat::IntR32G32B32A32:
                    return VK_FORMAT_R32G32B32A32_SINT;
                case nxt::VertexFormat::IntR32G32B32:
                    return VK_FORMAT_R32G32B32_SINT;
                case nxt::VertexFormat::IntR32G32:
                    return VK_FORMAT_R32G32_SINT;
                case nxt::VertexFormat::IntR32:
                    return VK_FORMAT_R32_SINT;
                case nxt::VertexFormat::UshortR16G16B16A16:
                    return VK_FORMAT_R16G16B16A16_UINT;
                case nxt::VertexFormat::UshortR16G16:
                    return VK_FORMAT_R16G16_UINT;
                case nxt::VertexFormat::UnormR8G8B8A8:
                    return VK_FORMAT_R8G8B8A8_UNORM;
                case nxt::VertexFormat::UnormR8G8:
                    return VK_FORMAT_R8G8_UNORM;
                default:
                    UNREACHABLE();
            }
        }

    }  // anonymous namespace

    InputState::InputState(InputStateBuilder* builder) : InputStateBase(builder) {
        // Fill in the "binding info" that will be chained in the create info
        uint32_t bindingCount = 0;
        for (uint32_t i : IterateBitSet(GetInputsSetMask())) {
            const auto& bindingInfo = GetInput(i);

            auto& bindingDesc = mBindings[bindingCount];
            bindingDesc.binding = i;
            bindingDesc.stride = bindingInfo.stride;
            bindingDesc.inputRate = VulkanInputRate(bindingInfo.stepMode);

            bindingCount++;
        }

        // Fill in the "attribute info" that will be chained in the create info
        uint32_t attributeCount = 0;
        for (uint32_t i : IterateBitSet(GetAttributesSetMask())) {
            const auto& attributeInfo = GetAttribute(i);

            auto& attributeDesc = mAttributes[attributeCount];
            attributeDesc.location = i;
            attributeDesc.binding = attributeInfo.bindingSlot;
            attributeDesc.format = VulkanVertexFormat(attributeInfo.format);
            attributeDesc.offset = attributeInfo.offset;

            attributeCount++;
        }

        // Build the create info
        mCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        mCreateInfo.pNext = nullptr;
        mCreateInfo.flags = 0;
        mCreateInfo.vertexBindingDescriptionCount = bindingCount;
        mCreateInfo.pVertexBindingDescriptions = mBindings.data();
        mCreateInfo.vertexAttributeDescriptionCount = attributeCount;
        mCreateInfo.pVertexAttributeDescriptions = mAttributes.data();
    }

    const VkPipelineVertexInputStateCreateInfo* InputState::GetCreateInfo() const {
        return &mCreateInfo;
    }

}}  // namespace backend::vulkan
