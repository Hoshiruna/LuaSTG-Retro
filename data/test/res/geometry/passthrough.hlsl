Texture2D screen_texture : register(t4);
SamplerState image_sampler : register(s4);
struct PS_Input
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

float4 main(PS_Input input) : SV_Target
{
    return screen_texture.Sample(image_sampler, input.uv);
}
