#version 430

// Input vertex attributes
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;

// Input uniform values
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;

// Output vertex attributes (to fragment shader)
out vec3 fragPosition;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;

void main()
{
	// Send vertex attributes to fragment shader
	fragPosition = vertexPosition;
	fragTexCoord = vertexTexCoord;
	fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 1)));

	// Calculate final vertex position
	gl_Position = mvp * vec4(vertexPosition, 1);
}
