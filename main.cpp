#include <raylib.h>
#include <raymath.h>
#include <rcamera.h>
#include <rlgl.h>

#define RLIGHTS_IMPLEMENTATION
#include <rlights.h>

#include <algorithm>
#include <assert.h>
#include <fstream>
#include <iomanip>
#include <istream>
#include <numeric>
#include <span>
#include <sstream>
#include <map>
#include <unordered_map>
#include <filesystem>

// n . x + d = 0
struct Plane
{
	Vector3 n;
	float d;
};

struct Face
{
	Plane plane;
	std::string texture_name;
	Vector2 texture_uv;
	float texture_rotation;
	Vector2 texture_scale;
};

struct Poly
{
	std::vector<Vector3> positions;
	std::string texture_name;
	std::vector<Vector2> texture_uvs;
};

struct Brush
{
	std::vector<Face> faces;
};

struct Entity
{
	std::unordered_map<std::string, std::string> tags;
	std::vector<Brush> brushes;
};

struct Map
{
	std::vector<Entity> entities;
};

using Texture_Map = std::map<std::string, Texture>;

void
UpdateCamera(Camera* camera);

std::vector<Model>
LoadModelsFromMapFile(const std::filesystem::path& path);

int
main()
{
	// SetTraceLogLevel(LOG_DEBUG);
	SetTargetFPS(60);
	SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
	InitWindow(1200, 800, "quake-level-viewer");
	DisableCursor();  // Limit cursor to relative movement inside the window
	rlDisableBackfaceCulling();
	rlEnableDepthMask();
	
	Camera camera = {
		.position = {10.0f, 10.0f, 10.0f},
		.target = {},
		.up = {0.0f, 1.0f, 0.0f},
		.fovy = 45.0f,
		.projection = CAMERA_PERSPECTIVE,
	};

	Shader shader = LoadShader(VS_PATH, FS_PATH);
	shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "viewPos");

	Light cameraLight = CreateLight(LIGHT_POINT, camera.position, {}, WHITE, shader);
	auto models = LoadModelsFromMapFile(MAP_SOURCE_DIR "/START.MAP");

	while (!WindowShouldClose())
	{
		static bool enable_lines = true;
		static bool enable_cursor = false;
		
		if (IsFileDropped())
		{
			FilePathList droppedFiles = LoadDroppedFiles();
			std::for_each(models.begin(), models.end(), UnloadModel);
			models = LoadModelsFromMapFile(droppedFiles.paths[0]);
			UnloadDroppedFiles(droppedFiles);
		}

		if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
		{
			enable_cursor = !enable_cursor;
			if (enable_cursor)
				EnableCursor();
			else
				DisableCursor();
		}

		if (IsKeyPressed(KEY_L))
			enable_lines = !enable_lines;

		UpdateCamera(&camera);
		cameraLight.position = camera.position;
		UpdateLightValues(shader, cameraLight);

		BeginDrawing();
		{
			ClearBackground(RAYWHITE);

			BeginMode3D(camera);
			{
				for (auto model : models)
				{
					DrawModel(model, {}, 1.f, WHITE);
					if (enable_lines)
						DrawModelWires(model, {}, 1.f, BLACK);
				}
				

				// DrawGrid(100 + (int)(Vector3Length(camera.position) / 10), 10.f);
				// DrawRay({.direction = {1, 0, 0}}, RED);
				// DrawRay({.direction = {0, 1, 0}}, GREEN);
				// DrawRay({.direction = {0, 0, 1}}, BLUE);
			}
			EndMode3D();

			DrawFPS(10, 10);

			DrawText("WASD (+SHIFT): Move, Mouse: Pan, SPACE/CTRL: Up/Down, L: Wireframe, RMB: Cursor", 10, GetScreenHeight() - 30, 20, DARKGRAY);
		}
		EndDrawing();
	}

	UnloadShader(shader);
	std::for_each(models.begin(), models.end(), UnloadModel);
	CloseWindow();
	return 0;
}

std::string
str_tolower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
	return s;
}

inline static Vector3
operator+(Vector3 a, Vector3 b)
{
	return Vector3Add(a, b);
}

inline static Vector3
operator-(Vector3 a, Vector3 b)
{
	return Vector3Subtract(a, b);
}

inline static Vector3
operator*(Vector3 a, float b)
{
	return Vector3Scale(a, b);
}

inline static Vector3
operator*(float a, Vector3 b)
{
	return b * a;
}

inline static Vector3
operator/(Vector3 a, float b)
{
	return Vector3Scale(a, 1.f / b);
}

Vector3
VerticesCenter(std::span<const Vector3> vertices)
{
	return std::reduce(vertices.begin(), vertices.end()) / vertices.size();
}

Vector3
VerticesNormal(std::span<const Vector3> vertices)
{
	Vector3 n = {};
	for (size_t i = 0; i < vertices.size(); i++)
	{
		size_t j = (i + 1) % vertices.size();

		Vector3 vi = vertices[i], vj = vertices[j];
		n.x += (vi.y - vj.y) * (vi.z + vj.z);
		n.y += (vi.z - vj.z) * (vi.x + vj.x);
		n.z += (vi.x - vj.x) * (vi.y + vj.y);
	}
	return Vector3Normalize(n);
}

Plane
PlaneFromVertices(std::span<const Vector3> vertices)
{
	Plane plane = {};
	plane.n = VerticesNormal(vertices);

	if (Vector3Equals(plane.n, {}) || Vector3Length(plane.n) <= EPSILON)
		return Plane{};

	plane.d = -Vector3DotProduct(VerticesCenter(vertices), plane.n);
	return plane;
}

Plane
PlaneFromTriangle(Vector3 v1, Vector3 v2, Vector3 v3)
{
	// Vertices are listed in counter-clockwise order
	// 0 ---------- 2
	// |
	// |
	// 1
	// Plane plane = {};
	// plane.n = Vector3Normalize(Vector3CrossProduct(v2 - v1, v3 - v1));
	// plane.d = -Vector3DotProduct(v1, plane.n);
	// return plane;
	Vector3 vertices[3] = {v1, v2, v3};
	return PlaneFromVertices(vertices);
}

float
PlaneSignedDistance(Plane p, Vector3 v)
{
	return Vector3DotProduct(p.n, v) + p.d;
}

std::pair<Vector3, bool>
GetIntersectionPlanes(Plane p1, Plane p2, Plane p3)
{
	float det = Vector3DotProduct(p1.n, Vector3CrossProduct(p2.n, p3.n));
	if (det == 0)
		return {Vector3{}, false};

	Vector3 out = (-p1.d * Vector3CrossProduct(p2.n, p3.n) +
					  -p2.d * Vector3CrossProduct(p3.n, p1.n) +
					  -p3.d * Vector3CrossProduct(p1.n, p2.n)) /
				  det;
	return {out, true};
}

std::pair<Vector3, Vector3>
TextureAxesFromPlane(Plane p)
{
	Vector3 baseAxis[] = {
		{0,0,1}, {1,0,0}, {0,-1,0},	 // floor
		{0,0,-1}, {1,0,0}, {0,-1,0}, // ceiling
		{1,0,0}, {0,1,0}, {0,0,-1},	 // west wall
		{-1,0,0}, {0,1,0}, {0,0,-1}, // east wall
		{0,1,0}, {1,0,0}, {0,0,-1},	 // south wall
		{0,-1,0}, {1,0,0}, {0,0,-1}, // north wall
	};

	Vector3 uNormal, vNormal;
	float maxDotProduct = -FLT_MAX;
	for (size_t i = 0; i < 6; i++)
	{
		auto dot = Vector3DotProduct(p.n, baseAxis[i * 3]);
		if (dot > maxDotProduct)
		{
			maxDotProduct = dot;
			uNormal = baseAxis[i * 3 + 1];
			vNormal = baseAxis[i * 3 + 2];
		}
	}
	return {uNormal, vNormal};
}

std::vector<Vector3>
PolySortVertices(const Poly &poly, Vector3 planeNormal)
{
	std::vector<Vector3> vertices(poly.positions.begin(), poly.positions.end());
	Vector3 center = VerticesCenter(vertices);

	for (auto it = vertices.begin(); (it + 2) != vertices.end(); it++)
	{
		Plane orthogonalPlane = PlaneFromTriangle(*it, center, center + planeNormal);
		Vector3 vN = *it - center;

		auto nearestVertex = std::min_element(it + 1, vertices.end(), [=](Vector3 a, Vector3 b) {
			if (PlaneSignedDistance(orthogonalPlane, a) < 0)  // On opposite sides of the plane from it
				return false;
			if (PlaneSignedDistance(orthogonalPlane, b) < 0)  // On opposite sides of the plane from it
				return true;

			Vector3 vA = a - center, vB = b - center;
			return Vector3Angle(vN, vA) < Vector3Angle(vN, vB);
		});
		std::swap(*(it + 1), *nearestVertex);
	}
	return vertices;
}

std::vector<Poly>
PolysFromFaces(const Texture_Map &texmap, std::span<const Face> faces)
{
	std::vector<Poly> polys(faces.size());
	for (size_t i = 0; i < faces.size(); i++)
	{
		for (size_t j = i + 1; j < faces.size(); j++)
		{
			for (size_t k = j + 1; k < faces.size(); k++)
			{
				bool legal = true;

				auto [vertex, ok] = GetIntersectionPlanes(faces[i].plane, faces[j].plane, faces[k].plane);
				if (ok == false)
					continue;

				for (size_t m = 0; m < faces.size(); m++)
				{
					if (PlaneSignedDistance(faces[m].plane, vertex) > 0)
					{
						legal = false;
						break;
					}
				}
				if (legal)
				{
					polys[i].positions.push_back(vertex);
					polys[j].positions.push_back(vertex);
					polys[k].positions.push_back(vertex);
				}
			}
		}
	}

	for (size_t i = 0; i < polys.size(); i++)
	{
		auto &poly = polys[i];
		auto &plane = faces[i].plane;

		if (poly.positions.size() < 3)
			continue;

		poly.positions = PolySortVertices(poly, plane.n);
		Vector3 polyNormal = VerticesNormal(poly.positions);
		if (Vector3DotProduct(plane.n, polyNormal) < 0)  // opposite sides
			std::reverse(poly.positions.begin(), poly.positions.end());

		// TODO: Constructive Solid Geometry Union (CSGUnion)
	}

	// Calculate texture coordinates
	for (size_t f = 0; f < polys.size(); f++)
	{
		auto& poly = polys[f];
		auto& face = faces[f];

		if (poly.positions.size() < 3)
			continue;

		auto& texture_data = texmap.at(str_tolower(face.texture_name));
		poly.texture_name = face.texture_name;
		Vector2 texture_size = {texture_data.width, texture_data.height};

		poly.texture_uvs.resize(poly.positions.size());
		for (size_t v = 0; v < poly.positions.size(); v++)
		{
			auto [uNormal, vNormal] = TextureAxesFromPlane(face.plane);
			Vector2 dot = {
				Vector3DotProduct(poly.positions[v], uNormal),
				Vector3DotProduct(poly.positions[v], vNormal),
			};
			
			Vector2 uv = Vector2Add(face.texture_uv, Vector2Divide(dot, face.texture_scale));
			poly.texture_uvs[v] = Vector2Divide(uv, texture_size);
		}
	}

	std::erase_if(polys, [](const Poly& p) { return p.positions.size() < 3; });
	return polys;
}

Vector3
ReadVector3(std::istream &stream)
{
	char token;
	stream >> token;
	if (token != '(')
		throw TextFormat("Expected ')', found %c", token);

	Vector3 v;
	stream >> v.x >> v.y >> v.z;

	stream >> token;
	if (token != ')')
		throw TextFormat("Expected ')', found %c", token);

	// IDEA: Divide components by scale?
	// Changing from Quake's (left-handed, Z-Up) system to Raylib's (right-handed, Y-Up) system
	return {v.x, v.z, -v.y};
}

Face
ReadFace(std::istream& stream)
{
	Face f = {};
	
	Vector3 pv[3];
	for (auto& v : pv)
		v = ReadVector3(stream);
	
	// Changing from Quake's (left-handed, Z-Up) system to Raylib's (right-handed, Y-Up) system
	f.plane = PlaneFromTriangle(pv[0], pv[2], pv[1]);

	stream >> f.texture_name;
	stream >> f.texture_uv.x >> f.texture_uv.y;
	stream >> f.texture_rotation;
	stream >> f.texture_scale.x >> f.texture_scale.y;

	return f;
}

Brush
ReadBrush(std::istream &stream)
{
	Brush brush{};

	char token;
	stream >> token;
	if (token != '{')
		throw TextFormat("Expected ')', found %c", token);

	while (stream >> std::ws)
	{
		token = stream.peek();
		if (token == '(')
		{
			brush.faces.push_back(ReadFace(stream));
		}
		else if (token == '}')
		{
			stream >> token;
			break;
		}
		else
			throw TextFormat("Expected ')', found %c", token);
	}
	return brush;
}

std::pair<std::string, std::string>
ReadProperty(std::istream &stream)
{
	std::string tag, tagValue;
	stream >> std::quoted(tag) >> std::quoted(tagValue);
	return {tag, tagValue};
}

Entity
ReadEntity(std::istream &stream)
{
	// TODO: Allow // comments
	Entity entity{};

	char token;
	stream >> token;
	if (token != '{')
		throw TextFormat("Expected ')', found %c", token);

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
			throw TextFormat("Expected ')', found %c", token);
	}

	return entity;
}

Map
ReadMap(std::istream &stream)
{
	Map map{};
	while ((stream >> std::ws).good())
	{
		Entity entity = ReadEntity(stream);
		assert(entity.tags.empty() == false);
		map.entities.push_back(entity);
	}
	return map;
}

Mesh
GenMeshPolygons(std::span<const Poly> polys)
{
	Mesh mesh{};
	for (auto &p : polys)
	{
		mesh.vertexCount += p.positions.size();
		mesh.triangleCount += p.positions.size() - 2;
	}

	// Vertices
	mesh.vertices = (float *)RL_MALLOC(mesh.vertexCount * 3 * sizeof(float));
	float *vertices = mesh.vertices;
	for (const auto &p : polys)
	{
		for (const auto &v : p.positions)
		{
			*(vertices++) = v.x;
			*(vertices++) = v.y;
			*(vertices++) = v.z;
		}
	}

	// Normals
	mesh.normals = (float *)RL_MALLOC(mesh.vertexCount * 3 * sizeof(float));
	float *normals = mesh.normals;
	for (auto &p : polys)
	{
		Vector3 n = VerticesNormal(p.positions);
		for (const auto &v : p.positions)
		{
			*(normals++) = n.x;
			*(normals++) = n.y;
			*(normals++) = n.z;
		}
	}

	// TexCoords
	mesh.texcoords = (float *)RL_MALLOC(mesh.vertexCount * 2 * sizeof(float));
	float *texcoords = mesh.texcoords;
	for (auto &p : polys)
	{
		for (const auto &v : p.positions)
		{
			*(texcoords++) = 0.f;
			*(texcoords++) = 0.f;
		}
	}

	// Indices
	mesh.indices = (unsigned short *)RL_MALLOC(mesh.triangleCount * 3 * sizeof(unsigned short));
	unsigned short *indices = mesh.indices;

	unsigned short pOffset = 0;
	for (auto &p : polys)
	{
		for (unsigned short i = 1; (i + 1) < p.positions.size(); i++)
		{
			*(indices++) = pOffset;
			*(indices++) = pOffset + i;
			*(indices++) = pOffset + i + 1;
		}
		pOffset += p.positions.size();
	}

	// Upload vertex data to GPU (static mesh)
	UploadMesh(&mesh, false);

	for (unsigned short i = 0; i < mesh.vertexCount; i++)
	{
		TraceLog(LOG_DEBUG, "[%d] v (%.3f %.3f %.3f), tc (%.3f %.3f), n (%.3f %.3f %.3f)",
			i,
			mesh.vertices[3 * i], mesh.vertices[3 * i + 1], mesh.vertices[3 * i + 2],
			mesh.texcoords[2 * i], mesh.texcoords[2 * i + 1],
			mesh.normals[3 * i], mesh.normals[3 * i + 1], mesh.normals[3 * i + 2]);
	}
	for (size_t i = 0; i < mesh.triangleCount; i++)
	{
		TraceLog(LOG_DEBUG, "idx (%d %d %d)",
			mesh.indices[3 * i], mesh.indices[3 * i + 1], mesh.indices[3 * i + 2]);
	}

	return mesh;
}

std::vector<Model>
LoadModelsFromMap(const Map &map, const Texture_Map &texmap)
{
	std::vector<Model> models;
	for (auto &entity : map.entities)
	{
		for (const auto &brush : entity.brushes)
		{
			auto polys = PolysFromFaces(texmap, brush.faces);
			if (polys.empty())
				continue;

			auto model = LoadModelFromMesh(GenMeshPolygons(polys));
			model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texmap.at(str_tolower(polys[0].texture_name));
			models.push_back(model);
		}
	}
	return models;
}

void
UpdateCamera(Camera *camera)
{
	float CAMERA_MOVE_SPEED = 0.03f;
	constexpr float CAMERA_ROTATION_SPEED = 0.03f;
	constexpr float CAMERA_MOUSE_MOVE_SENSITIVITY = 0.003f;

	if (IsKeyDown(KEY_LEFT_SHIFT))
		CAMERA_MOVE_SPEED *= 10;

	Vector2 mousePositionDelta = GetMouseDelta();

	bool lockView = true;
	bool rotateAroundTarget = false;
	bool rotateUp = false;
	bool moveInWorldPlane = true;

	// Camera rotation
	if (IsKeyDown(KEY_DOWN))
		CameraPitch(camera, -CAMERA_ROTATION_SPEED, lockView, rotateAroundTarget, rotateUp);
	if (IsKeyDown(KEY_UP))
		CameraPitch(camera, CAMERA_ROTATION_SPEED, lockView, rotateAroundTarget, rotateUp);
	if (IsKeyDown(KEY_RIGHT))
		CameraYaw(camera, -CAMERA_ROTATION_SPEED, rotateAroundTarget);
	if (IsKeyDown(KEY_LEFT))
		CameraYaw(camera, CAMERA_ROTATION_SPEED, rotateAroundTarget);
	// if (IsKeyDown(KEY_Q)) CameraRoll(camera, -CAMERA_ROTATION_SPEED);
	// if (IsKeyDown(KEY_E)) CameraRoll(camera, CAMERA_ROTATION_SPEED);

	CameraYaw(camera, -mousePositionDelta.x * CAMERA_MOUSE_MOVE_SENSITIVITY, rotateAroundTarget);
	CameraPitch(camera, -mousePositionDelta.y * CAMERA_MOUSE_MOVE_SENSITIVITY, lockView, rotateAroundTarget, rotateUp);

	// Camera movement
	if (IsKeyDown(KEY_W))
		CameraMoveForward(camera, 20 * CAMERA_MOVE_SPEED, moveInWorldPlane);
	if (IsKeyDown(KEY_A))
		CameraMoveRight(camera, -20 * CAMERA_MOVE_SPEED, moveInWorldPlane);
	if (IsKeyDown(KEY_S))
		CameraMoveForward(camera, -20 * CAMERA_MOVE_SPEED, moveInWorldPlane);
	if (IsKeyDown(KEY_D))
		CameraMoveRight(camera, 20 * CAMERA_MOVE_SPEED, moveInWorldPlane);
	if (IsKeyDown(KEY_SPACE))
		CameraMoveUp(camera, 20 * CAMERA_MOVE_SPEED);
	if (IsKeyDown(KEY_LEFT_CONTROL))
		CameraMoveUp(camera, -20 * CAMERA_MOVE_SPEED);
}

#pragma pack(push, 1)
struct WAD_Header
{
	char magic[4];
	int32_t entries_count;
	int32_t directory_offset;
};

#define MAXTEXNAME 16
#define MIPLEVELS 4
struct WAD_Entry
{
	int32_t offset;
	int32_t size_in_file;
	int32_t size_in_memory;
	int8_t type;
	int8_t compression;
	int16_t _dummy;
	char name[MAXTEXNAME];
};

struct Color_RGB8
{
	uint8_t r, g, b;
};

struct WAD_Palette
{
	Color_RGB8 c[256];
};

struct WAD_Texture_Header
{
	char name[MAXTEXNAME];
	uint32_t width, height;
	uint32_t mipmap_offsets[MIPLEVELS];
};
#pragma pack(pop)

template <typename T>
T
ReadT(std::istream& stream)
{
	T data{};
	stream.read((char *)&data, sizeof(T));

	size_t last_read = stream.gcount();
	return data;
}

Texture_Map
ReadTextures(std::istream &stream)
{
	Texture_Map texmap{};
	
	auto header = ReadT<WAD_Header>(stream);
	assert(strncmp(header.magic, "WAD2", 4) == 0);
	stream.seekg(header.directory_offset);
	
	// OPT: Read entire directory in one go
	std::vector<WAD_Entry> entries(header.entries_count);
	for (size_t i = 0; i < header.entries_count; i++)
	{
		assert(stream.good());
		entries[i] = ReadT<WAD_Entry>(stream);
	}

	WAD_Palette palette{{
		{  0,  0,  0}, { 15, 15, 15}, { 31, 31, 31}, { 47, 47, 47}, { 63, 63, 63},
		{ 75, 75, 75}, { 91, 91, 91}, {107,107,107}, {123,123,123}, {139,139,139},
		{155,155,155}, {171,171,171}, {187,187,187}, {203,203,203}, {219,219,219},
		{235,235,235}, { 15, 11,  7}, { 23, 15, 11}, { 31, 23, 11}, { 39, 27, 15},
		{ 47, 35, 19}, { 55, 43, 23}, { 63, 47, 23}, { 75, 55, 27}, { 83, 59, 27},
		{ 91, 67, 31}, { 99, 75, 31}, {107, 83, 31}, {115, 87, 31}, {123, 95, 35},
		{131,103, 35}, {143,111, 35}, { 11, 11, 15}, { 19, 19, 27}, { 27, 27, 39},
		{ 39, 39, 51}, { 47, 47, 63}, { 55, 55, 75}, { 63, 63, 87}, { 71, 71,103},
		{ 79, 79,115}, { 91, 91,127}, { 99, 99,139}, {107,107,151}, {115,115,163},
		{123,123,175}, {131,131,187}, {139,139,203}, {  0,  0,  0}, {  7,  7,  0},
		{ 11, 11,  0}, { 19, 19,  0}, { 27, 27,  0}, { 35, 35,  0}, { 43, 43,  7},
		{ 47, 47,  7}, { 55, 55,  7}, { 63, 63,  7}, { 71, 71,  7}, { 75, 75, 11},
		{ 83, 83, 11}, { 91, 91, 11}, { 99, 99, 11}, {107,107, 15}, {  7,  0,  0},
		{ 15,  0,  0}, { 23,  0,  0}, { 31,  0,  0}, { 39,  0,  0}, { 47,  0,  0},
		{ 55,  0,  0}, { 63,  0,  0}, { 71,  0,  0}, { 79,  0,  0}, { 87,  0,  0},
		{ 95,  0,  0}, {103,  0,  0}, {111,  0,  0}, {119,  0,  0}, {127,  0,  0},
		{ 19, 19,  0}, { 27, 27,  0}, { 35, 35,  0}, { 47, 43,  0}, { 55, 47,  0},
		{ 67, 55,  0}, { 75, 59,  7}, { 87, 67,  7}, { 95, 71,  7}, {107, 75, 11},
		{119, 83, 15}, {131, 87, 19}, {139, 91, 19}, {151, 95, 27}, {163, 99, 31},
		{175,103, 35}, { 35, 19,  7}, { 47, 23, 11}, { 59, 31, 15}, { 75, 35, 19},
		{ 87, 43, 23}, { 99, 47, 31}, {115, 55, 35}, {127, 59, 43}, {143, 67, 51},
		{159, 79, 51}, {175, 99, 47}, {191,119, 47}, {207,143, 43}, {223,171, 39},
		{239,203, 31}, {255,243, 27}, { 11,  7,  0}, { 27, 19,  0}, { 43, 35, 15},
		{ 55, 43, 19}, { 71, 51, 27}, { 83, 55, 35}, { 99, 63, 43}, {111, 71, 51},
		{127, 83, 63}, {139, 95, 71}, {155,107, 83}, {167,123, 95}, {183,135,107},
		{195,147,123}, {211,163,139}, {227,179,151}, {171,139,163}, {159,127,151},
		{147,115,135}, {139,103,123}, {127, 91,111}, {119, 83, 99}, {107, 75, 87},
		{ 95, 63, 75}, { 87, 55, 67}, { 75, 47, 55}, { 67, 39, 47}, { 55, 31, 35},
		{ 43, 23, 27}, { 35, 19, 19}, { 23, 11, 11}, { 15,  7,  7}, {187,115,159},
		{175,107,143}, {163, 95,131}, {151, 87,119}, {139, 79,107}, {127, 75, 95},
		{115, 67, 83}, {107, 59, 75}, { 95, 51, 63}, { 83, 43, 55}, { 71, 35, 43},
		{ 59, 31, 35}, { 47, 23, 27}, { 35, 19, 19}, { 23, 11, 11}, { 15,  7,  7},
		{219,195,187}, {203,179,167}, {191,163,155}, {175,151,139}, {163,135,123},
		{151,123,111}, {135,111, 95}, {123, 99, 83}, {107, 87, 71}, { 95, 75, 59},
		{ 83, 63, 51}, { 67, 51, 39}, { 55, 43, 31}, { 39, 31, 23}, { 27, 19, 15},
		{ 15, 11,  7}, {111,131,123}, {103,123,111}, { 95,115,103}, { 87,107, 95},
		{ 79, 99, 87}, { 71, 91, 79}, { 63, 83, 71}, { 55, 75, 63}, { 47, 67, 55},
		{ 43, 59, 47}, { 35, 51, 39}, { 31, 43, 31}, { 23, 35, 23}, { 15, 27, 19},
		{ 11, 19, 11}, {  7, 11,  7}, {255,243, 27}, {239,223, 23}, {219,203, 19},
		{203,183, 15}, {187,167, 15}, {171,151, 11}, {155,131,  7}, {139,115,  7},
		{123, 99,  7}, {107, 83,  0}, { 91, 71,  0}, { 75, 55,  0}, { 59, 43,  0},
		{ 43, 31,  0}, { 27, 15,  0}, { 11,  7,  0}, {  0,  0,255}, { 11, 11,239},
		{ 19, 19,223}, { 27, 27,207}, { 35, 35,191}, { 43, 43,175}, { 47, 47,159},
		{ 47, 47,143}, { 47, 47,127}, { 47, 47,111}, { 47, 47, 95}, { 43, 43, 79},
		{ 35, 35, 63}, { 27, 27, 47}, { 19, 19, 31}, { 11, 11, 15}, { 43,  0,  0},
		{ 59,  0,  0}, { 75,  7,  0}, { 95,  7,  0}, {111, 15,  0}, {127, 23,  7},
		{147, 31,  7}, {163, 39, 11}, {183, 51, 15}, {195, 75, 27}, {207, 99, 43},
		{219,127, 59}, {227,151, 79}, {231,171, 95}, {239,191,119}, {247,211,139},
		{167,123, 59}, {183,155, 55}, {199,195, 55}, {231,227, 87}, {127,191,255},
		{171,231,255}, {215,255,255}, {103,  0,  0}, {139,  0,  0}, {179,  0,  0},
		{215,  0,  0}, {255,  0,  0}, {255,243,147}, {255,247,199}, {255,255,255},
		{159, 91, 83},
	}};

	for (const auto &entry : entries)
	{
		stream.seekg(entry.offset);
		assert(stream.good());

		switch (entry.type)
		{
		case 0x40:  // Color Palette
		{
			palette = ReadT<WAD_Palette>(stream);
			break;
		}
		case 0x42:  // Pictures
		{
			auto width = ReadT<uint32_t>(stream);
			auto height = ReadT<uint32_t>(stream);
			stream.seekg(width * height, std::ios_base::cur);
			break;
		}
		case 0x44:  // Mip Texture
		{
			auto tx = ReadT<WAD_Texture_Header>(stream);
			stream.seekg(entry.offset + tx.mipmap_offsets[0]);

			std::vector<uint8_t> texture_palette_idx(tx.height * tx.width);
			stream.read((char*)texture_palette_idx.data(), texture_palette_idx.size());
			assert(stream.gcount() == texture_palette_idx.size());

			std::vector<Color_RGB8> texture_color;
			for (auto p : texture_palette_idx)
				texture_color.push_back(palette.c[p]);

			Image texture_image = {
				.data = texture_color.data(),
				.width = (int)tx.width,
				.height = (int)tx.height,
				.mipmaps = 1,
				.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8,
			};
			texmap[str_tolower(tx.name)] = LoadTextureFromImage(texture_image);
			break;
		}
		default:
			assert(false);
		}
	}

	return texmap;
}

std::vector<Model>
LoadModelsFromMapFile(const std::filesystem::path& path)
{
	std::ifstream mapFile{path};
	auto map = ReadMap(mapFile);

	auto wadPath = path.parent_path() / map.entities[0].tags.at("wad");
	std::ifstream wadFile{wadPath, std::ios_base::binary};
	auto texmap = ReadTextures(wadFile);
	return LoadModelsFromMap(map, texmap);
}