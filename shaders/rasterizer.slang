module rasterizer;

struct Vertex
{
    float3 position;
    float3 color;
};

[shader("compute")]
[numthreads(8, 8, 1)]
void draw(
    uint3 id : SV_DispatchThreadID,
    uniform uint triangle,
    in StructuredBuffer<Vertex> vertices,
    in StructuredBuffer<uint> indices,
    in RWTexture2D<float4> colorOutput,
    in RWTexture2D<float> depthOutput
) {
    uint width = 0;
    uint height = 0;
    colorOutput.GetDimensions(width, height);

    if (id.x >= width || id.y >= height)
        return;

    float2 pixelCenter = float2(id.xy) + 0.5;

    uint i0 = indices[triangle * 3 + 0];
    uint i1 = indices[triangle * 3 + 1];
    uint i2 = indices[triangle * 3 + 2];

    Vertex v0 = vertices[i0];
    Vertex v1 = vertices[i1];
    Vertex v2 = vertices[i2];

    float3 p0 = v0.position;
    float3 p1 = v1.position;
    float3 p2 = v2.position;

    // Compute barycentric coordinates
    float area = ((p1.x - p0.x) * (p2.y - p0.y)) - ((p2.x - p0.x) * (p1.y - p0.y));
    float w0 = ((p1.x - pixelCenter.x) * (p2.y - pixelCenter.y)) - ((p2.x - pixelCenter.x) * (p1.y - pixelCenter.y));
    float w1 = ((p2.x - pixelCenter.x) * (p0.y - pixelCenter.y)) - ((p0.x - pixelCenter.x) * (p2.y - pixelCenter.y));
    float w2 = ((p0.x - pixelCenter.x) * (p1.y - pixelCenter.y)) - ((p1.x - pixelCenter.x) * (p0.y - pixelCenter.y));

    if (area == 0.0)
        return; // Degenerate triangle

    w0 /= area;
    w1 /= area;
    w2 /= area;

    // Check if the point is inside the triangle
    if (w0 < 0.0 || w1 < 0.0 || w2 < 0.0)
        return;

    // Interpolate depth
    float depth = w0 * p0.z + w1 * p1.z + w2 * p2.z;

    // Depth test
    float currentDepth = depthOutput[id.xy];
    if (depth < currentDepth)
    {
        depthOutput[id.xy] = depth;

        // Interpolate color
        float3 color = w0 * v0.color + w1 * v1.color + w2 * v2.color;
        colorOutput[id.xy] = float4(color, 1.0);
    }
}