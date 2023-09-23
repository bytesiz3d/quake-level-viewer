#include <raylib.h>
#include <raymath.h>
#include <rcamera.h>
#include <rlgl.h>

#include <assert.h>
#include <format>
#include <fstream>
#include <iomanip>
#include <istream>
#include <span>
#include <sstream>
#include <unordered_map>

#include "raymath-extras.h"

#if defined(PLATFORM_DESKTOP)
#define GLSL_VERSION 330
#else // PLATFORM_RPI, PLATFORM_ANDROID, PLATFORM_WEB
#define GLSL_VERSION 100
#endif

struct Poly
{
	std::vector<Vector3> v;
	BoundingBox bbox;
};

std::vector<Poly>
PolysFromPlanes(std::span<Plane> planes)
{
	std::vector<Poly> polys(planes.size());
	for (size_t i = 0; i < planes.size() - 2; i++)
	{
		for (size_t j = i + 1; j < planes.size() - 1; j++)
		{
			for (size_t k = j + 1; k < planes.size(); k++)
			{
				bool legal = true;

				Vector3 vertex = {};
				if (GetIntersectionPlanes(planes[i], planes[j], planes[k], vertex) == false)
					continue;

				for (size_t m = 0; m < planes.size(); m++)
				{
					if (PlaneSignedDistance(planes[m], vertex) > 0)
					{
						legal = false;
						break;
					}
				}
				if (legal)
				{
					polys[i].v.push_back(vertex);
					polys[j].v.push_back(vertex);
					polys[k].v.push_back(vertex);
				}
			}
		}
	}

	for (auto& poly : polys)
	{
		if (poly.v.empty())
			continue;

		Vector3 minVertex = poly.v[0];
		Vector3 maxVertex = poly.v[0];

		for (size_t i = 1; i < poly.v.size(); i++)
		{
			minVertex = Vector3Min(minVertex, poly.v[i]);
			maxVertex = Vector3Max(maxVertex, poly.v[i]);
		}
		poly.bbox = {.min = minVertex, .max = maxVertex};
	}
	return polys;
}

Plane
ReadPlane(std::istream& stream)
{
	char token;

	Vector3 pv[3];
	for (auto& v : pv)
	{
		stream >> token;
		if (token != '(')
			throw std::format("Expected '(', found {}", token);

		// NOTE: Divide components by scale?
		stream >> v.x >> v.y >> v.z;

		stream >> token;
		if (token != ')')
			throw std::format("Expected ')', found {}", token);
	}

	// skip over the remaining data
	char buf[256];
	stream.getline(buf, sizeof(buf));

	// NOTE: Vertices are listed in clockwise order
	// 0 ---------- 1
	// |
	// |
	// 2
	Plane plane = {};
	plane.n = Vector3Normalize(cross(pv[2] - pv[0], pv[1] - pv[0]));
	plane.d = -dot(plane.n, pv[0]);
	return plane;
}

struct Brush
{
	std::vector<Plane> planes;
	std::vector<Poly> polys;
	BoundingBox bbox;
};

Brush
ReadBrush(std::istream& stream)
{
	std::vector<Plane> planes;

	char token;
	stream >> token;
	if (token != '{')
		throw std::format("Expected '{{', found {}", token);

	while (stream >> std::ws)
	{
		token = stream.peek();
		if (token == '(')
		{
			auto plane = ReadPlane(stream);
			planes.push_back(plane);
		}
		else if (token == '}')
		{
			stream >> token;
			break;
		}
		else
			throw std::format("Unexpected token {}", token);
	}

	auto polys = PolysFromPlanes(planes);
	return Brush{planes, polys};
}


struct Entity
{
	std::unordered_map<std::string, std::string> tags;
	std::vector<Brush> brushes;
};

std::pair<std::string, std::string>
ReadProperty(std::istream& stream)
{
	std::string tag, tagValue;
	stream >> std::quoted(tag) >> std::quoted(tagValue);
	return {tag, tagValue};
}

Entity
ReadEntity(std::istream& stream)
{
	Entity entity{};

	char token;
	stream >> token;
	if (token != '{')
		throw std::format("Expected '{{', found {}", token);

	while (stream >> std::ws)
	{
		token = stream.peek();
		if (token == '"')
		{
			auto [tag, tagValue] = ReadProperty(stream);
			entity.tags[tag] = tagValue;
		}
		else if (token == '{')
		{
			entity.brushes.push_back(ReadBrush(stream));
		}
		else if (token == '}')
		{
			stream >> token;
			break;
		}
		else
			throw std::format("Unexpected token {}", token);
	}

	return entity;
}


struct Map
{
	std::vector<Entity> entities;
};

Map
ReadMap(std::istream& stream)
{
	Map map = {};
	while ((stream >> std::ws).good())
	{
		Entity entity = ReadEntity(stream);
		assert(entity.tags.empty() == false);
		map.entities.push_back(entity);
	}
	return map;
}

Map
LoadMapFile(const char* path)
{
	std::ifstream mapFile{path};
	return ReadMap(mapFile);
}

void
UpdateCamera(Camera* camera)
{
	constexpr float CAMERA_MOVE_SPEED = 0.3f;
	constexpr float CAMERA_ROTATION_SPEED = 0.03f;
	constexpr float CAMERA_MOUSE_MOVE_SENSITIVITY = 0.003f;

	Vector2 mousePositionDelta = GetMouseDelta();

	bool lockView = true;
	bool rotateAroundTarget = false;
	bool rotateUp = false;
	bool moveInWorldPlane = true;

	// Camera rotation
	if (IsKeyDown(KEY_DOWN)) CameraPitch(camera, -CAMERA_ROTATION_SPEED, lockView, rotateAroundTarget, rotateUp);
	if (IsKeyDown(KEY_UP)) CameraPitch(camera, CAMERA_ROTATION_SPEED, lockView, rotateAroundTarget, rotateUp);
	if (IsKeyDown(KEY_RIGHT)) CameraYaw(camera, -CAMERA_ROTATION_SPEED, rotateAroundTarget);
	if (IsKeyDown(KEY_LEFT)) CameraYaw(camera, CAMERA_ROTATION_SPEED, rotateAroundTarget);
	// if (IsKeyDown(KEY_Q)) CameraRoll(camera, -CAMERA_ROTATION_SPEED);
	// if (IsKeyDown(KEY_E)) CameraRoll(camera, CAMERA_ROTATION_SPEED);

	CameraYaw(camera, -mousePositionDelta.x * CAMERA_MOUSE_MOVE_SENSITIVITY, rotateAroundTarget);
	CameraPitch(camera, -mousePositionDelta.y * CAMERA_MOUSE_MOVE_SENSITIVITY, lockView, rotateAroundTarget, rotateUp);

	// Camera movement
	if (IsKeyDown(KEY_W)) CameraMoveForward(camera, 20 * CAMERA_MOVE_SPEED, moveInWorldPlane);
	if (IsKeyDown(KEY_A)) CameraMoveRight(camera, -20 * CAMERA_MOVE_SPEED, moveInWorldPlane);
	if (IsKeyDown(KEY_S)) CameraMoveForward(camera, -20 * CAMERA_MOVE_SPEED, moveInWorldPlane);
	if (IsKeyDown(KEY_D)) CameraMoveRight(camera, 20 * CAMERA_MOVE_SPEED, moveInWorldPlane);
	if (IsKeyDown(KEY_SPACE)) CameraMoveUp(camera, 20 * CAMERA_MOVE_SPEED);
	if (IsKeyDown(KEY_LEFT_CONTROL)) CameraMoveUp(camera, -20 * CAMERA_MOVE_SPEED);
}

int
main()
{
	SetTargetFPS(60);
	SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
	InitWindow(1600, 900, "quake-level-viewer");
	DisableCursor(); // Limit cursor to relative movement inside the window

	auto map = LoadMapFile(MAP_SOURCE_DIR "/DM1.MAP");

	Camera camera = {
		.position = {1.0f, 1.0f, 1.0f},
		.target = {4.0f, 1.0f, 4.0f},
		.up = {0.0f, 1.0f, 0.0f},
		.fovy = 45.0f,
		.projection = CAMERA_PERSPECTIVE,
	};

	rlEnableDepthMask();
	while (!WindowShouldClose())
	{
		if (IsFileDropped())
		{
			FilePathList droppedFiles = LoadDroppedFiles();
			map = LoadMapFile(droppedFiles.paths[0]);
			UnloadDroppedFiles(droppedFiles);
		}

		UpdateCamera(&camera);

		BeginDrawing();
		{
			ClearBackground(RAYWHITE);

			BeginMode3D(camera);
			{
				for (auto& entity : map.entities)
				{
					for (auto& brush : entity.brushes)
					{
						for (auto& poly : brush.polys)
						{
							DrawBoundingBox(poly.bbox, LIME);
							DrawCubeV(BboxCenter(poly.bbox), BboxSize(poly.bbox), DARKGRAY);
						}
					}
				}
				DrawGrid(100 + (int)(Vector3Length(camera.position) / 10), 10.f);
			}
			EndMode3D();

			DrawFPS(10, 10);
			DrawText("WASD: Move, Mouse: Pan, SPACE, CTRL: Up/Down", 10, GetScreenHeight() - 30, 20, DARKGRAY);
		}
		EndDrawing();
	}

	CloseWindow();
	return 0;
}

enum class ENTITY_KIND
{
	worldspawn,

	air_bubbles, // Rising bubbles

	ambient_drip,        // Dripping sound
	ambient_drone,       // Engine/machinery sound
	ambient_comp_hum,    // Computer background sounds
	ambient_flouro_buzz, // Flourescent buzzing sound
	ambient_light_buzz,  // Buzzing sound from light
	ambient_suck_wind,   // Wind sound
	ambient_swamp1,      // Frogs croaking
	ambient_swamp2,      // Slightly different sounding frogs croaking
	ambient_thunder,     // Thunder sound

	event_lightning, // Lightning (Used to kill Cthon, shareware boss)

	func_door,        // Door
	func_door_secret, // A door that is triggered to open
	func_wall,        // A moving wall?
	func_button,      // A button
	func_train,       // A platform (moves along a "train")
	func_plat,        // A lift/elevator
	func_dm_only,     // A teleporter that only appears in deathmatch
	func_illusionary, // Creates brush that appears solid, but isn't.

	info_null,                 // Used as a placeholder (removes itself)
	info_notnull,              // Used as a placeholder (does not remove itself)
	info_intermission,         // Cameras positioning for intermission (?)
	info_player_start,         // Main player starting point (only one allowed)
	info_player_deathmatch,    // A deathmatch start (more than one allowed)
	info_player_coop,          // A coop player start (more than one allowed)
	info_player_start2,        // Return point from episode
	info_teleport_destination, // Gives coords for a teleport destination using a targetname

	// All item_ tags may have a target tag. It triggers the event when the item is picked up.

	item_cells,                    // Ammo for the Thunderbolt
	item_rockets,                  // Ammo for Rocket/Grenade Launcher
	item_shells,                   // Ammo for both Shotgun and SuperShotgun
	item_spikes,                   // Ammo for Perforator and Super Perforator
	item_weapon,                   // Generic weapon class
	item_health,                   // Medkit
	item_artifact_envirosuit,      // Environmental Protection Suit
	item_artifact_super_damage,    // Quad Damage
	item_artifact_invulnerability, // Pentagram of Protection
	item_artifact_invisibility,    // Ring of Shadows (Invisibility)
	item_armorInv,                 // Red armor
	item_armor2,                   // Yellow armor
	item_armor1,                   // Green armor
	item_key1,                     // Silver Key
	item_key2,                     // Gold Key
	item_sigil,                    // Sigil (a rune)

	light,                       // A projected light. No visible lightsource.
	light_torch_small_walltorch, // Small wall torch (gives off light)
	light_flame_large_yellow,    // Large yellow fire (gives off light)
	light_flame_small_yellow,    // Small yellow fire (gives off light)
	light_flame_small_white,     // Small white fire  (gives off light)
	light_fluoro,                // Fluorescent light? (Gives off light, humming sound?)
	light_fluorospark,           // Fluorescent light? (Gives off light, makes sparking sound)
	light_globe,                 // Light that appears as a globe sprite

	monster_army,          // Grunt
	monster_dog,           // Attack dog
	monster_ogre,          // Ogre
	monster_ogre_marksman, // Ogre (synonymous with monster_ogre)
	monster_knight,        // Knight
	monster_zombie,        // Zombie
	monster_wizard,        // Scragg (Wizard)
	monster_demon1,        // Fiend (Demon)
	monster_shambler,      // Shambler
	monster_boss,          // Cthon (Boss of Shareware Quake)
	monster_enforcer,      // Enforcer
	monster_hell_knight,   // Hell Knight
	monster_shalrath,      // Shalrath
	monster_tarbaby,       // Slime
	monster_fish,          // Fish
	monster_oldone,        // Shubb-Niggurath (requires a misc_teleportrain and a info_intermission)

	misc_fireball,      // Small fireball (gives off light, harms player)
	misc_explobox,      // Large Nuclear Container
	misc_explobox2,     // Small Nuclear Container
	misc_teleporttrain, // Spiked ball needed to telefrag monster_oldone

	path_corner, // Used to define path of func_train platforms

	trap_spikeshooter, // Shoots spikes (nails)
	trap_shooter,      // Fires nails without needing to be triggered.

	trigger_teleport,       // Teleport (all trigger_ tags are triggered by walkover)
	trigger_changelevel,    // Changes to another level
	trigger_setskill,       // Changes skill level
	trigger_counter,        // Triggers action after it has been triggered count times.
	trigger_once,           // Triggers action only once
	trigger_multiple,       // Triggers action (can be retriggered)
	trigger_onlyregistered, // Triggers only if game is registered (registered == 1)
	trigger_secret,         // Triggers action and awards secret credit.
	trigger_monsterjump,    // Causes triggering monster to jump in a direction
	trigger_relay,          // Allows delayed/multiple actions from one trigger
	trigger_push,           // Pushes a player in a direction (like a windtunnel)
	trigger_hurt,           // Hurts whatever touches the trigger

	weapon_supershotgun,    // Super Shotgun
	weapon_nailgun,         // Perforator
	weapon_supernailgun,    // Super Perforator
	weapon_grenadelauncher, // Grenade Launcher
	weapon_rocketlauncher,  // Rocket Launcher
	weapon_lightning,       // Lightning Guna
};
