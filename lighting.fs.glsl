#version 330

// Input vertex attributes (from vertex shader)
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec3 fragNormal;
in vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Output fragment color
out vec4 finalColor;

// NOTE: Add here your custom variables

#define     MAX_LIGHTS              4
#define     LIGHT_DIRECTIONAL       0
#define     LIGHT_POINT             1

struct Light
{
	int enabled;
	int type;
	vec3 position;
	vec3 target;
	vec4 color;
};

// Input lighting values
uniform Light lights[MAX_LIGHTS];
uniform vec3 viewPos;
uniform int lightPower;

float
Attenuate(float distance)
{
	float HD = lightPower;
	float K = 1.0 / (HD * HD);
	float dsq = distance * distance;
	return 1.0 / (1.0 + K*dsq);
}

void main()
{
	// Texel color fetching from texture sampler
	vec4 texelColor = texture(texture0, fragTexCoord);
	vec3 normal = normalize(fragNormal);
	vec3 ambient = vec3(0.01);

	// NOTE: Implement here your fragment shader code

	vec3 lightDot = vec3(0.0);
	for (int i = 0; i < MAX_LIGHTS; i++)
	{
		if (lights[i].enabled == 1)
		{
			vec3 light_direction = vec3(0.0);
			float light_distance = 0;

			if (lights[i].type == LIGHT_DIRECTIONAL)
			{
				light_direction = normalize(lights[i].target - lights[i].position);
				light_distance = distance(lights[i].target, lights[i].position);
			}
			else if (lights[i].type == LIGHT_POINT)
			{
				light_direction = normalize(fragPosition - lights[i].position);
				light_distance = distance(fragPosition, lights[i].position);
			}

			float NdotL = abs(dot(normal, light_direction)) * Attenuate(light_distance);
			lightDot += lights[i].color.rgb * NdotL;
		}
	}

	finalColor  = texelColor * colDiffuse * vec4(lightDot, 1.0);
	finalColor += texelColor * colDiffuse * vec4(ambient, 1.0);
	finalColor *= fragColor;

	// Gamma correction
	finalColor = pow(finalColor, vec4(1.0/2.2));
}
