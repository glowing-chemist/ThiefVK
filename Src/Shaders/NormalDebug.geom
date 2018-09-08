#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(triangles) in;

layout(location = 0) in vec3 Norms[];

layout(location = 4) out vec3 lineColour;
layout(line_strip, max_vertices = 6) out;


void main() {
	for(int vertex = 0; vertex < 3; ++vertex) {
		gl_Position = gl_in[vertex].gl_Position;
		lineColour = vec3(0.2);
		EmitVertex(); // emit the start of the line.

		gl_Position = gl_in[vertex].gl_Position + vec4(Norms[vertex], 0.0f);
		lineColour = vec3(0.6);
		EmitVertex(); // end of the line.

		EndPrimitive();
	}
}