#ifndef OX_MAX_TEXTURES
#define OX_MAX_TEXTURES 32
#endif

struct Frame
{
    float4x4 view_projection;
};

ConstantBuffer<Frame> frame;
