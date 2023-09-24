#version 330

precision mediump float;

in vec3 fragPosition;

// Input uniform values
uniform vec3 cameraPosition;

// Output fragment color
out vec4 finalColor;

float
attenuate_light(vec3 position, vec3 cameraPos)
{
	float HD = 30.0;
	float K = 1.0 / (HD * HD);
	float distance = distance(position, cameraPos);
	return 1.0 / (1.0 + K * distance * distance);
}

void
main()
{
	float light = 0.8 * attenuate_light(fragPosition, cameraPosition);
    finalColor = vec4(light, light, light, 1.0);
}
