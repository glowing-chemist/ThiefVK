#version 450
#extension GL_ARB_separate_shader_objects : enable


out gl_PerVertex {
    vec4 gl_Position;
};


void main()
{
	// Pass through the coordinates for a full screen triangle.
    float x = -1.0 + float((gl_VertexIndex & 1) << 2);
    float y = -1.0 + float((gl_VertexIndex & 2) << 1);
    gl_Position = vec4(x, y, 0, 1);
}