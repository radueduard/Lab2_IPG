#include "IPG_Lab2.h"

#include <algorithm>

#include <glm/gtc/type_ptr.hpp>

#include <IconsFontAwesome6.h>
#include <imgui_internal.h>

namespace
{
    constexpr ImVec2 ROW_PADDING { 8.f, 5.f };
    constexpr float ROW_ROUNDING = 4.f;
    constexpr float ROW_SPACING = 4.f;
    constexpr float SWATCH_SIZE = 12.f;

    // Transparent at rest so the delete button reads as part of the selected row it sits on,
    // then destructive red once the cursor is actually on it.
    constexpr ImVec4 DELETE_IDLE { 0.f, 0.f, 0.f, 0.f };
    constexpr ImVec4 DELETE_HOVERED { 0.78f, 0.24f, 0.24f, 1.f };
    constexpr ImVec4 DELETE_ACTIVE { 0.56f, 0.14f, 0.14f, 1.f };

    constexpr ImWchar TRASH_GLYPH = 0xf1f8;  // ICON_FA_TRASH

    constexpr float DEFAULT_DEPTH = 0.2f;

    // New vertices cascade instead of landing on the same spot, so three added in a row are a
    // usable triangle rather than three coincident points.
    constexpr float CASCADE_STEP = 40.f;
    constexpr int CASCADE_PERIOD = 8;

    struct RowResult {
        bool clicked = false;
        bool deleted = false;
    };

    // One list row: rounded highlight, a label, an optional colour swatch, and a delete button
    // that only appears on the selected row. Shared by the vertex and triangle lists so the two
    // cannot drift apart visually.
    RowResult DrawRow(const char* label, const bool isSelected, const ImVec4* swatch = nullptr)
    {
        RowResult result;

        const float rowWidth = ImGui::GetContentRegionAvail().x;
        const float rowHeight = ImGui::GetTextLineHeight() + ROW_PADDING.y * 2.f;
        const ImVec2 rowMin = ImGui::GetCursorScreenPos();
        const ImVec2 rowMax { rowMin.x + rowWidth, rowMin.y + rowHeight };

        const bool isHovered = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(rowMin, rowMax);

        // Drawn before the Selectable, which is then rendered with transparent highlights, so
        // the rounding is ours - a Selectable's own background is always square.
        if (isSelected || isHovered)
            ImGui::GetWindowDrawList()->AddRectFilled(rowMin, rowMax,
                ImGui::GetColorU32(isSelected ? ImGuiCol_Header : ImGuiCol_HeaderHovered), ROW_ROUNDING);

        ImGui::PushStyleColor(ImGuiCol_Header, 0);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, 0);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, 0);
        // AllowOverlap so the delete button, drawn after, takes the clicks it sits on top of.
        result.clicked = ImGui::Selectable("##row", isSelected, ImGuiSelectableFlags_AllowOverlap,
                                           { rowWidth, rowHeight });
        ImGui::PopStyleColor(3);

        // The row is one Selectable, so its contents are positioned by hand over the top of it.
        const ImVec2 afterRow = ImGui::GetCursorScreenPos();

        float textX = rowMin.x + ROW_PADDING.x;

        if (swatch != nullptr)
        {
            const ImVec2 swatchMin { textX, rowMin.y + (rowHeight - SWATCH_SIZE) * 0.5f };
            ImGui::GetWindowDrawList()->AddRectFilled(swatchMin,
                { swatchMin.x + SWATCH_SIZE, swatchMin.y + SWATCH_SIZE },
                ImGui::GetColorU32(*swatch), 3.f);
            textX += SWATCH_SIZE + ROW_PADDING.x;
        }

        ImGui::SetCursorScreenPos({ textX, rowMin.y + ROW_PADDING.y });
        ImGui::TextUnformatted(label);

        if (isSelected)
        {
            const float button = rowHeight - ROW_PADDING.y;
            ImGui::SetCursorScreenPos({ rowMax.x - button - ROW_PADDING.x, rowMin.y + (rowHeight - button) * 0.5f });

            // The button carries no label: letting it draw one centres the glyph's *advance*
            // box, and an icon sits on the text baseline, so its ink is not centred within
            // that box - which leaves the trash visibly low. The glyph is drawn separately
            // below, positioned off its ink rectangle instead.
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ROW_ROUNDING);
            ImGui::PushStyleColor(ImGuiCol_Button, DELETE_IDLE);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, DELETE_HOVERED);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, DELETE_ACTIVE);

            result.deleted = ImGui::Button("##delete", { button, button });

            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();

            if (const ImFontGlyph* glyph = ImGui::GetFont()->FindGlyph(TRASH_GLYPH))
            {
                // Glyph corners are in the atlas's native font size, so rescale to the size
                // actually being rendered at.
                const float glyphScale = ImGui::GetFontSize() / ImGui::GetFont()->FontSize;
                const ImVec2 buttonMin = ImGui::GetItemRectMin();
                const float inkWidth = (glyph->X1 - glyph->X0) * glyphScale;
                const float inkHeight = (glyph->Y1 - glyph->Y0) * glyphScale;

                // Solve for the pen position that lands the ink centred, rather than the pen.
                const ImVec2 pen {
                    buttonMin.x + (button - inkWidth) * 0.5f - glyph->X0 * glyphScale,
                    buttonMin.y + (button - inkHeight) * 0.5f - glyph->Y0 * glyphScale
                };

                ImGui::GetWindowDrawList()->AddText(pen, ImGui::GetColorU32(ImGuiCol_Text), ICON_FA_TRASH);
            }
        }

        ImGui::SetCursorScreenPos(afterRow);

        return result;
    }

    bool EditVertex(IPG_Lab2::Vertex& vertex)
    {
        bool changed = false;

        // Positions are left unclamped: the shader bounds-checks per pixel, so a vertex may
        // sit off-canvas and only part of the triangle land in the image.
        changed |= ImGui::DragFloat2("Position", glm::value_ptr(vertex.position), 1.f, 0.f, 0.f, "%.0f");

        // Depth *is* clamped: the clear value is 1.0 and the test is a plain less-than, so a
        // vertex outside [0, 1] would interpolate to fragments that can never pass it.
        changed |= ImGui::SliderFloat("Depth", &vertex.position.z, 0.f, 1.f, "%.3f");

        changed |= ImGui::ColorEdit3("Color", glm::value_ptr(vertex.color));

        return changed;
    }
}

void IPG_Lab2::Initialize()
{
    const auto shader = kor::Shader::Builder()
        .setLang<kor::Shader::Lang::eSlang>()
        .setEntryPoint("rasterizer", "draw")
        .getOrBuild();

    pipeline = kor::ComputePipeline::Builder()
        .setComputeShader(shader)
        .build();

    colorImage = kor::Image::Builder()
        .setIsPerFrame(true)
        .setFormat(kor::Image::Format::eRGBA8_UNORM)
        .setExtent(CANVAS_SIZE)
        .addUsage(kor::Image::Usage::eStorage)
        .addUsage(kor::Image::Usage::eTransferSrc)
        .addUsage(kor::Image::Usage::eTransferDst)
        .build();

    // A storage image, not a depth attachment: the shader reads and writes it as
    // RWTexture2D<float>, and a D32_SFLOAT image cannot be bound that way. That also means the
    // depth clear in Render is an ordinary colour clear of a single-channel float image.
    depthImage = kor::Image::Builder()
        .setIsPerFrame(true)
        .setFormat(kor::Image::Format::eR32_SFLOAT)
        .setExtent(CANVAS_SIZE)
        .addUsage(kor::Image::Usage::eStorage)
        .addUsage(kor::Image::Usage::eTransferDst)
        .build();

    colorImageView = kor::ImageView::Builder(colorImage).build();
    depthImageView = kor::ImageView::Builder(depthImage).build();

    vertexBuffer = kor::Buffer::Builder<Vertex>()
        .setInstanceCount(MAX_VERTICES)
        .setIsPerFrame(true)
        .addUsage(kor::Buffer::Usage::eStorage)
        .setType(kor::Buffer::Type::eDynamic)
        .build();

    indexBuffer = kor::Buffer::Builder<Triangle>()
        .setInstanceCount(MAX_TRIANGLES)
        .setIsPerFrame(true)
        .addUsage(kor::Buffer::Usage::eStorage)
        .setType(kor::Buffer::Type::eDynamic)
        .build();

    descriptorSet = kor::DescriptorSet::Builder(pipeline, 0)
        .write(0, kor::Descriptor(vertexBuffer))
        .write(1, kor::Descriptor(indexBuffer))
        .write(2, kor::Descriptor(colorImageView))
        .write(3, kor::Descriptor(depthImageView))
        .build();

    vertices = {
        { { 290.f, 630.f, 0.5f }, { 1.f, 0.f, 0.f } },
        { { 1099.f, 270.f, 0.5f }, { 0.f, 1.f, 0.f } },
        { { 650.f, 1.f, 0.5f }, { 0.f, 0.f, 1.f } },

        { { 200.f, 270.f, 0.f }, { 0.f, 1.f, 1.f } },
        { { 830.f, 1.f, 1.f }, { 1.f, 1.f, 0.f } },
        { { 1099.f, 720.f, 1.f }, { 1.f, 0.f, 1.f } },
    };

    triangles = {
        { 0, 1, 2 },
        { 3, 4, 5 },
    };

    selection = Selection::Triangle;
    selectedIndex = 0;

    UploadMesh();
}

void IPG_Lab2::UploadMesh()
{
    if (!vertices.empty())
        vertexBuffer->Write(std::span<const Vertex> { vertices });

    if (!triangles.empty())
        indexBuffer->Write(std::span<const Triangle> { triangles });
}

void IPG_Lab2::Update()
{
    if (meshDirty)
    {
        UploadMesh();
        meshDirty = false;
    }
}

void IPG_Lab2::Render(kor::CommandBuffer& commandBuffer)
{
    const auto indices = std::views::iota(0u, static_cast<glm::uint>(triangles.size()));

    commandBuffer
        .ClearColorImage(colorImage, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f))
        .ClearColorImage(depthImage, glm::vec4(1.0f))
        .BindComputePipeline(pipeline)
        .BindDescriptorSet(0, descriptorSet)
        .ForEach(indices, [&](auto& cb, glm::uint i) {
            cb
                .PushConstants(i)
                .Dispatch(CANVAS_SIZE.x / 8, CANVAS_SIZE.y / 8);
        })
        .Blit(colorImage);
}

void IPG_Lab2::RenderUI()
{
    if (!dockLayoutBuilt && meshDockId != 0)
    {
        dockLayoutBuilt = true;

        if (propertiesDockId == 0)
        {
            ImGuiID top, bottom;
            ImGui::DockBuilderSplitNode(meshDockId, ImGuiDir_Down, 0.45f, &bottom, &top);
            ImGui::DockBuilderDockWindow("Mesh", top);
            ImGui::DockBuilderDockWindow("Properties", bottom);
            ImGui::DockBuilderFinish(meshDockId);
        }
    }

    const auto vertexCount = static_cast<int>(vertices.size());
    const auto triangleCount = static_cast<int>(triangles.size());

    int deleteVertex = -1;
    int deleteTriangle = -1;

    ImGui::Begin("Mesh");
    meshDockId = ImGui::GetWindowDockID();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { ImGui::GetStyle().ItemSpacing.x, ROW_SPACING });

    ImGui::SeparatorText("Vertices");

    ImGui::BeginDisabled(vertexCount >= MAX_VERTICES);
    if (ImGui::Button(ICON_FA_PLUS "  Add vertex"))
    {
        const float cascade = static_cast<float>(vertexCount % CASCADE_PERIOD) * CASCADE_STEP;
        vertices.push_back({ { cascade, cascade, DEFAULT_DEPTH }, { 1.f, 1.f, 1.f } });
        selection = Selection::Vertex;
        selectedIndex = static_cast<int>(vertices.size()) - 1;
        meshDirty = true;
    }
    ImGui::EndDisabled();

    if (vertexCount >= MAX_VERTICES && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("The vertex buffer holds at most %d vertices", MAX_VERTICES);

    ImGui::PushID("vertices");
    for (int i = 0; i < vertexCount; ++i)
    {
        ImGui::PushID(i);

        char label[64];
        std::snprintf(label, sizeof(label), "%d  (%.0f, %.0f)", i, vertices[i].position.x, vertices[i].position.y);

        const glm::vec3& c = vertices[i].color;
        const ImVec4 swatch { c.r, c.g, c.b, 1.f };

        const RowResult row = DrawRow(label, selection == Selection::Vertex && selectedIndex == i, &swatch);

        if (row.clicked)
        {
            selection = Selection::Vertex;
            selectedIndex = i;
        }

        if (row.deleted)
            deleteVertex = i;

        ImGui::PopID();
    }
    ImGui::PopID();

    ImGui::Spacing();
    ImGui::SeparatorText("Triangles");

    const bool canAddTriangle = vertexCount >= 3 && triangleCount < MAX_TRIANGLES;

    ImGui::BeginDisabled(!canAddTriangle);
    if (ImGui::Button(ICON_FA_PLUS "  Add triangle"))
    {
        // Seeded from the three most recent vertices, which is what "add three vertices, then
        // a triangle" is reaching for. Any corner can be repointed afterwards.
        const auto last = static_cast<glm::uint>(vertexCount - 1);
        triangles.push_back({ last - 2, last - 1, last });
        selection = Selection::Triangle;
        selectedIndex = static_cast<int>(triangles.size()) - 1;
        meshDirty = true;
    }
    ImGui::EndDisabled();

    if (!canAddTriangle && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip(vertexCount < 3 ? "A triangle needs at least 3 vertices"
                                          : "The index buffer holds at most %d triangles", MAX_TRIANGLES);

    ImGui::PushID("triangles");
    for (int i = 0; i < triangleCount; ++i)
    {
        ImGui::PushID(i);

        const Triangle& t = triangles[i];

        char label[64];
        std::snprintf(label, sizeof(label), "%d  -  %u, %u, %u", i, t.x, t.y, t.z);

        const RowResult row = DrawRow(label, selection == Selection::Triangle && selectedIndex == i);

        if (row.clicked)
        {
            selection = Selection::Triangle;
            selectedIndex = i;
        }

        if (row.deleted)
            deleteTriangle = i;

        ImGui::PopID();
    }
    ImGui::PopID();

    ImGui::PopStyleVar();

    if (triangles.empty())
        ImGui::TextDisabled("No triangles yet.");

    ImGui::End();

    ImGui::Begin("Properties");
    propertiesDockId = ImGui::GetWindowDockID();

    if (selection == Selection::Vertex && selectedIndex >= 0 && selectedIndex < vertexCount)
    {
        // How many triangles reference this vertex decides whether editing it is a local edit
        // or one that moves several triangles at once - worth saying before the drag, not after.
        const auto users = std::ranges::count_if(triangles, [&](const Triangle& t) {
            return t.x == static_cast<glm::uint>(selectedIndex)
                || t.y == static_cast<glm::uint>(selectedIndex)
                || t.z == static_cast<glm::uint>(selectedIndex);
        });

        ImGui::Text("Vertex %d", selectedIndex);
        ImGui::SameLine();
        if (users == 0)
            ImGui::TextDisabled("(unused)");
        else if (users == 1)
            ImGui::TextDisabled("(used by 1 triangle)");
        else
            ImGui::TextDisabled("(shared by %lld triangles)", static_cast<long long>(users));

        ImGui::Spacing();

        meshDirty |= EditVertex(vertices[selectedIndex]);
    }
    else if (selection == Selection::Triangle && selectedIndex >= 0 && selectedIndex < triangleCount)
    {
        Triangle& triangle = triangles[selectedIndex];

        ImGui::Text("Triangle %d", selectedIndex);
        ImGui::Spacing();

        for (int corner = 0; corner < 3; ++corner)
        {
            ImGui::PushID(corner);

            char header[32];
            std::snprintf(header, sizeof(header), "Corner %d", corner + 1);
            ImGui::SeparatorText(header);

            // Edited as a plain index into `vertices`, clamped to what exists: an out-of-range
            // index would have the shader read past the end of the buffer.
            int index = static_cast<int>(triangle[corner]);
            if (ImGui::DragInt("Vertex", &index, 0.1f, 0, vertexCount - 1))
            {
                triangle[corner] = static_cast<glm::uint>(std::clamp(index, 0, vertexCount - 1));
                meshDirty = true;
            }

            // The referenced vertex is editable inline, so a triangle can be shaped without
            // hopping back to the vertex list for each corner.
            meshDirty |= EditVertex(vertices[triangle[corner]]);

            ImGui::PopID();
        }
    }
    else
    {
        ImGui::TextDisabled("Select a vertex or a triangle to edit it.");
    }

    ImGui::End();

    if (deleteVertex >= 0)
    {
        // Every triangle that used this vertex loses a corner and cannot survive it, so those
        // go too; the rest have their higher indices pulled down to match the shortened array.
        const auto removed = static_cast<glm::uint>(deleteVertex);

        std::erase_if(triangles, [&](const Triangle& t) {
            return t.x == removed || t.y == removed || t.z == removed;
        });

        for (Triangle& t : triangles)
            for (int corner = 0; corner < 3; ++corner)
                if (t[corner] > removed)
                    --t[corner];

        vertices.erase(vertices.begin() + deleteVertex);

        // The triangle list may have shrunk too, so any triangle selection is no longer
        // trustworthy - drop back to no selection rather than guess at a survivor.
        if (selection == Selection::Triangle)
            selection = Selection::None;
        else if (selectedIndex > deleteVertex)
            --selectedIndex;
        else if (selectedIndex == deleteVertex)
            selectedIndex = std::min(selectedIndex, static_cast<int>(vertices.size()) - 1);

        if (vertices.empty() && selection == Selection::Vertex)
            selection = Selection::None;

        meshDirty = true;
    }
    else if (deleteTriangle >= 0)
    {
        triangles.erase(triangles.begin() + deleteTriangle);

        if (selection == Selection::Triangle)
        {
            if (triangles.empty())
                selection = Selection::None;
            else if (selectedIndex > deleteTriangle)
                --selectedIndex;
            else if (selectedIndex == deleteTriangle)
                selectedIndex = std::min(selectedIndex, static_cast<int>(triangles.size()) - 1);
        }

        meshDirty = true;
    }
}
