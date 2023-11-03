#include <raylib.h>
#include <raymath.h>
#include <rcamera.h>
#include <rlgl.h>

// @REF https://github.com/raysan5/raylib/blob/master/examples/shaders/rlights.h
#define RLIGHTS_IMPLEMENTATION
#include <rlights.h>

#include <imgui.h>
#include <rlImGui.h>

#include <filesystem>
#include <set>

std::vector<Model>
LoadModelsFromBSPFile(const std::filesystem::path& path);

int
main()
{
	SetTargetFPS(60);
	SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
	InitWindow(1200, 800, "quake-level-viewer");
	SetWindowState(FLAG_WINDOW_MAXIMIZED);
	rlEnableBackfaceCulling();
	rlImGuiSetup(false);

	std::vector<Model> models = LoadModelsFromBSPFile(MAP_SOURCE_DIR "/bsp/dm4.bsp");
	
	long shaderModTime = std::max(GetFileModTime(VS_PATH), GetFileModTime(FS_PATH));
	Shader shader = LoadShader(VS_PATH, FS_PATH);
	shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "viewPos");

	Camera camera = {
		.position = {10.0f, 10.0f, 10.0f},
		.target = {},
		.up = {0.0f, 1.0f, 0.0f},
		.fovy = 90.f,
		.projection = CAMERA_PERSPECTIVE,
	};
	Light cameraLight = CreateLight(LIGHT_POINT, camera.position, {}, WHITE, shader);
	int lightPower = 10;
	SetShaderValue(shader, GetShaderLocation(shader, "lightPower"), &lightPower, SHADER_UNIFORM_INT);

	DisableCursor(); // Limit cursor to relative movement inside the window
	while (!WindowShouldClose())
	{
		// Check if shader file has been modified
		long currentShaderModTime = std::max(GetFileModTime(VS_PATH), GetFileModTime(FS_PATH));
		if (currentShaderModTime != shaderModTime)
		{
			// Try hot-reloading updated shader
			Shader updatedShader = LoadShader(VS_PATH, FS_PATH);
			if (updatedShader.id != rlGetShaderIdDefault()) // It was correctly loaded
			{
				UnloadShader(shader);
				shader = updatedShader;
				shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "viewPos");
				
				lightsCount = 0;
				cameraLight = CreateLight(LIGHT_POINT, camera.position, {}, WHITE, shader);
				SetShaderValue(shader, GetShaderLocation(shader, "lightPower"), &lightPower, SHADER_UNIFORM_INT);
			}

			shaderModTime = currentShaderModTime;
		}

		if (IsFileDropped())
		{
			FilePathList droppedFiles = LoadDroppedFiles();
			
			std::set<decltype(Texture::id)> textures{};
			for (Model model : models)
			{
				Texture texture = model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture;
				textures.insert(texture.id);
			}
			std::for_each(std::begin(textures), std::end(textures), rlUnloadTexture);
			
			std::for_each(models.begin(), models.end(), UnloadModel);
			models = LoadModelsFromBSPFile(droppedFiles.paths[0]);
			UnloadDroppedFiles(droppedFiles);
		}

		static bool enable_cursor = false;
		if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
		{
			if (enable_cursor = !enable_cursor)
				EnableCursor();
			else
				DisableCursor();
		}
		if (enable_cursor == false)
			UpdateCamera(&camera, CAMERA_FREE);

		static bool enable_wireframe = false;
		if (IsKeyPressed(KEY_L))
			enable_wireframe = !enable_wireframe;

		static bool enable_imgui = true;
		if (IsKeyPressed(KEY_I))
			enable_imgui = !enable_imgui;
		
		cameraLight.position = camera.position;
		UpdateLightValues(shader, cameraLight);

		BeginDrawing();
		{
			ClearBackground(GRAY);
			
			BeginMode3D(camera);
			{
				for (auto model : models)
				{
					model.materials[0].shader = shader;
					DrawModel(model, {}, 1.f, WHITE);
					if (enable_wireframe)
						DrawModelWires(model, {}, 1.f, BLACK);
				}
			}
			EndMode3D();

			if (enable_imgui)
			{
				rlImGuiBegin();
				if (ImGui::Begin("Controls"))
				{
					ImGui::TextWrapped("FPS: %d", GetFPS());
					ImGui::BulletText("WASD:        Move");
					ImGui::BulletText("SPACE/LCTRL: Up/Down");
					ImGui::BulletText("Mouse:       Pan");
					ImGui::BulletText("L:           Toggle Wireframe");
					ImGui::BulletText("I:           Toggle UI");
					ImGui::BulletText("RMB:         Toggle Cursor");

					if (ImGui::SliderInt("Light Power", &lightPower, 1, 50))
						SetShaderValue(shader, GetShaderLocation(shader, "lightPower"), &lightPower, SHADER_UNIFORM_INT);
				}
				ImGui::End();
				rlImGuiEnd();
			}
		}
		EndDrawing();
	}

	UnloadShader(shader);
	std::for_each(models.begin(), models.end(), UnloadModel);
	rlImGuiShutdown();
	CloseWindow();
	return 0;
}