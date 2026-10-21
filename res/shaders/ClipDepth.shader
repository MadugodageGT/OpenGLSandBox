#shader vertex
#version 330 core

layout(location = 0) in vec3 a_Position;

uniform mat4 u_MVP;

void main()
{
    gl_Position = u_MVP * vec4(a_Position, 1.0);
}

#shader fragment
#version 330 core

// Depth-only pass used to rasterise the surface in view space. With u_Peel == 1 every fragment that
// belongs to the nearest layer (already stored in u_PrevDepth) is discarded, so the depth buffer ends up
// holding the SECOND layer (depth peeling) - this is what reveals a back surface inside a prism.
uniform int u_Peel;
uniform float u_PeelEps;
uniform sampler2D u_PrevDepth;

void main()
{
    if (u_Peel == 1)
    {
        float prev = texelFetch(u_PrevDepth, ivec2(gl_FragCoord.xy), 0).r;
        if (gl_FragCoord.z <= prev + u_PeelEps)
            discard;
    }
}
