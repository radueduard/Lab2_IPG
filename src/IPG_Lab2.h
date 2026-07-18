#pragma once

#include <koral.h>

class IPG_Lab2 final : public kor::Scene
{
public:
    void Initialize() override;
    void Update() override;
    void Render(kor::CommandBuffer& commandBuffer) override;
    void RenderUI(ImGuiContext* context) override;
};
