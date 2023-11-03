#include <config.h>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <set>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include <span>

#include <assert.h>

template<typename T>
T
ReadT(std::istream& stream)
{
	T data{};
	stream.read((char*)&data, sizeof(T));
	return data;
}

template<typename T>
T
ReadT(std::istream& stream, size_t idx)
{
	stream.seekg(idx * sizeof(T), std::ios::cur);
	return ReadT<T>(stream);
}

#pragma pack(push, 1)

struct Color_RGB8
{
	uint8_t r, g, b;
};

Color_RGB8
palette(uint8_t id)
{
	static const Color_RGB8 _PALETTE[] = {
		{0, 0, 0},
		{15, 15, 15},
		{31, 31, 31},
		{47, 47, 47},
		{63, 63, 63},
		{75, 75, 75},
		{91, 91, 91},
		{107, 107, 107},
		{123, 123, 123},
		{139, 139, 139},
		{155, 155, 155},
		{171, 171, 171},
		{187, 187, 187},
		{203, 203, 203},
		{219, 219, 219},
		{235, 235, 235},
		{15, 11, 7},
		{23, 15, 11},
		{31, 23, 11},
		{39, 27, 15},
		{47, 35, 19},
		{55, 43, 23},
		{63, 47, 23},
		{75, 55, 27},
		{83, 59, 27},
		{91, 67, 31},
		{99, 75, 31},
		{107, 83, 31},
		{115, 87, 31},
		{123, 95, 35},
		{131, 103, 35},
		{143, 111, 35},
		{11, 11, 15},
		{19, 19, 27},
		{27, 27, 39},
		{39, 39, 51},
		{47, 47, 63},
		{55, 55, 75},
		{63, 63, 87},
		{71, 71, 103},
		{79, 79, 115},
		{91, 91, 127},
		{99, 99, 139},
		{107, 107, 151},
		{115, 115, 163},
		{123, 123, 175},
		{131, 131, 187},
		{139, 139, 203},
		{0, 0, 0},
		{7, 7, 0},
		{11, 11, 0},
		{19, 19, 0},
		{27, 27, 0},
		{35, 35, 0},
		{43, 43, 7},
		{47, 47, 7},
		{55, 55, 7},
		{63, 63, 7},
		{71, 71, 7},
		{75, 75, 11},
		{83, 83, 11},
		{91, 91, 11},
		{99, 99, 11},
		{107, 107, 15},
		{7, 0, 0},
		{15, 0, 0},
		{23, 0, 0},
		{31, 0, 0},
		{39, 0, 0},
		{47, 0, 0},
		{55, 0, 0},
		{63, 0, 0},
		{71, 0, 0},
		{79, 0, 0},
		{87, 0, 0},
		{95, 0, 0},
		{103, 0, 0},
		{111, 0, 0},
		{119, 0, 0},
		{127, 0, 0},
		{19, 19, 0},
		{27, 27, 0},
		{35, 35, 0},
		{47, 43, 0},
		{55, 47, 0},
		{67, 55, 0},
		{75, 59, 7},
		{87, 67, 7},
		{95, 71, 7},
		{107, 75, 11},
		{119, 83, 15},
		{131, 87, 19},
		{139, 91, 19},
		{151, 95, 27},
		{163, 99, 31},
		{175, 103, 35},
		{35, 19, 7},
		{47, 23, 11},
		{59, 31, 15},
		{75, 35, 19},
		{87, 43, 23},
		{99, 47, 31},
		{115, 55, 35},
		{127, 59, 43},
		{143, 67, 51},
		{159, 79, 51},
		{175, 99, 47},
		{191, 119, 47},
		{207, 143, 43},
		{223, 171, 39},
		{239, 203, 31},
		{255, 243, 27},
		{11, 7, 0},
		{27, 19, 0},
		{43, 35, 15},
		{55, 43, 19},
		{71, 51, 27},
		{83, 55, 35},
		{99, 63, 43},
		{111, 71, 51},
		{127, 83, 63},
		{139, 95, 71},
		{155, 107, 83},
		{167, 123, 95},
		{183, 135, 107},
		{195, 147, 123},
		{211, 163, 139},
		{227, 179, 151},
		{171, 139, 163},
		{159, 127, 151},
		{147, 115, 135},
		{139, 103, 123},
		{127, 91, 111},
		{119, 83, 99},
		{107, 75, 87},
		{95, 63, 75},
		{87, 55, 67},
		{75, 47, 55},
		{67, 39, 47},
		{55, 31, 35},
		{43, 23, 27},
		{35, 19, 19},
		{23, 11, 11},
		{15, 7, 7},
		{187, 115, 159},
		{175, 107, 143},
		{163, 95, 131},
		{151, 87, 119},
		{139, 79, 107},
		{127, 75, 95},
		{115, 67, 83},
		{107, 59, 75},
		{95, 51, 63},
		{83, 43, 55},
		{71, 35, 43},
		{59, 31, 35},
		{47, 23, 27},
		{35, 19, 19},
		{23, 11, 11},
		{15, 7, 7},
		{219, 195, 187},
		{203, 179, 167},
		{191, 163, 155},
		{175, 151, 139},
		{163, 135, 123},
		{151, 123, 111},
		{135, 111, 95},
		{123, 99, 83},
		{107, 87, 71},
		{95, 75, 59},
		{83, 63, 51},
		{67, 51, 39},
		{55, 43, 31},
		{39, 31, 23},
		{27, 19, 15},
		{15, 11, 7},
		{111, 131, 123},
		{103, 123, 111},
		{95, 115, 103},
		{87, 107, 95},
		{79, 99, 87},
		{71, 91, 79},
		{63, 83, 71},
		{55, 75, 63},
		{47, 67, 55},
		{43, 59, 47},
		{35, 51, 39},
		{31, 43, 31},
		{23, 35, 23},
		{15, 27, 19},
		{11, 19, 11},
		{7, 11, 7},
		{255, 243, 27},
		{239, 223, 23},
		{219, 203, 19},
		{203, 183, 15},
		{187, 167, 15},
		{171, 151, 11},
		{155, 131, 7},
		{139, 115, 7},
		{123, 99, 7},
		{107, 83, 0},
		{91, 71, 0},
		{75, 55, 0},
		{59, 43, 0},
		{43, 31, 0},
		{27, 15, 0},
		{11, 7, 0},
		{0, 0, 255},
		{11, 11, 239},
		{19, 19, 223},
		{27, 27, 207},
		{35, 35, 191},
		{43, 43, 175},
		{47, 47, 159},
		{47, 47, 143},
		{47, 47, 127},
		{47, 47, 111},
		{47, 47, 95},
		{43, 43, 79},
		{35, 35, 63},
		{27, 27, 47},
		{19, 19, 31},
		{11, 11, 15},
		{43, 0, 0},
		{59, 0, 0},
		{75, 7, 0},
		{95, 7, 0},
		{111, 15, 0},
		{127, 23, 7},
		{147, 31, 7},
		{163, 39, 11},
		{183, 51, 15},
		{195, 75, 27},
		{207, 99, 43},
		{219, 127, 59},
		{227, 151, 79},
		{231, 171, 95},
		{239, 191, 119},
		{247, 211, 139},
		{167, 123, 59},
		{183, 155, 55},
		{199, 195, 55},
		{231, 227, 87},
		{127, 191, 255},
		{171, 231, 255},
		{215, 255, 255},
		{103, 0, 0},
		{139, 0, 0},
		{179, 0, 0},
		{215, 0, 0},
		{255, 0, 0},
		{255, 243, 147},
		{255, 247, 199},
		{255, 255, 255},
		{159, 91, 83},
	};
	return _PALETTE[id];
}

struct Vector3S
{
	int16_t x, y, z;
};

struct BoundingBoxS // Bounding Box, Short values
{
	Vector3S min, max;
};

struct Dir_Entry // A Directory entry
{
	int32_t offset; // Offset to entry, in bytes, from start of file
	int32_t size;   // Size of entry in file, in bytes
};

struct Header // The BSP file header
{
	int32_t version; // Model version, must be 0x17 (23).

	Dir_Entry entities;   // List of Entities.
	Dir_Entry planes;     // Map Planes.
						  // numplanes = size/sizeof(plane_t)
	Dir_Entry miptex;     // Wall Textures.
	Dir_Entry vertices;   // Map Vertices.
						  // numvertices = size/sizeof(vertex_t)
	Dir_Entry visibility; // Leaves Visibility lists.
	Dir_Entry nodes;      // BSP Nodes.
						  // numnodes = size/sizeof(node_t)
	Dir_Entry texinfos;   // Texture Info for faces.
						  // numtexinfo = size/sizeof(texinfo_t)
	Dir_Entry faces;      // Faces of each surface.
						  // numfaces = size/sizeof(face_t)
	Dir_Entry lightmaps;  // Wall Light Maps.
	Dir_Entry clipnodes;  // clip nodes, for Models.
						  // numclips = size/sizeof(clipnode_t)
	Dir_Entry leaves;     // BSP Leaves.
						  // numleaves = size/sizeof(leaf_t)
	Dir_Entry listfaces;  // List of Faces.
	Dir_Entry edges;      // Edges of faces.
						  // numedges = size/sizeof(edge_t)
	Dir_Entry listedges;  // List of Edges.
	Dir_Entry models;     // List of Models.
						  // nummodels = Size/sizeof(model_t)
};

struct Entity
{
	std::unordered_map<std::string, std::string> tags;
};

struct BSP_Model
{
	BoundingBox bound;    // The bounding box of the Model
	Vector3 origin;       // origin of model, usually (0,0,0)
	int32_t bsp_node_id;  // index of first BSP node
	int32_t clipnode1_id; // index of the first Clip node
	int32_t clipnode2_id; // index of the second Clip node
	int32_t _dummy_id;    // usually zero
	int32_t numleafs;     // number of BSP leaves
	int32_t face_id;      // index of Faces
	int32_t face_num;     // number of Faces
};

struct Edge
{
	uint16_t vs; // index of the start vertex
		//  must be in [0,numvertices[
	uint16_t ve; // index of the end vertex
				 //  must be in [0,numvertices[
};

struct Plane
{
	Vector3 normal; // Vector orthogonal to plane (Nx,Ny,Nz)
	float dist;     // Offset to plane, along the normal vector.
	int32_t type;   // Type of plane, depending on normal vector.
};

struct TexInfo
{
	Vector3 u_axis;     // U vector, horizontal in texture space)
	float u_offset;     // horizontal offset in texture space
	Vector3 v_axis;     // V vector, vertical in texture space
	float v_offset;     // vertical offset in texture space
	uint32_t miptex_id; // Index of Mip Texture
						//           must be in [0,numtex[
	uint32_t animated;  // 0 for ordinary textures, 1 for water
};

struct Face
{
	uint16_t plane_id;   // The plane in which the face lies
						 //           must be in [0,numplanes[
	uint16_t side;       // 0 if in front of the plane, 1 if behind the plane
	int32_t ledge_id;    // first edge in the List of edges
						 //           must be in [0,numledges[
	uint16_t ledge_num;  // number of edges in the List of edges
	uint16_t texinfo_id; // index of the Texture info the face is part of
						 //           must be in [0,numtexinfos[
	uint8_t typelight;   // type of lighting, for the face
	uint8_t baselight;   // from 0xFF (dark) to 0 (bright)
	uint8_t light[2];    // two additional light models
	uint32_t lightmap;   // Pointer inside the general light map, or -1
						 // this define the start of the face light map
};

struct Miptex // Mip Texture
{
	char name[16];      // Name of the texture.
	uint32_t width;     // width of picture, must be a multiple of 8
	uint32_t height;    // height of picture, must be a multiple of 8
	uint32_t offset[4]; // offsets to uint8_t Pix[width * height], relative to start of Miptex
};

/**
* typedef struct                 // Mip texture list header
* { int32_t numtex;                 // Number of textures in Mip Texture list
*   int32_t offset[numtex];         // Offset to each of the individual texture from the beginning of mipheader_t
* } mipheader_t;
*/

struct Node
{
	uint32_t plane_id; // The plane that splits the node
					   //           must be in [0,numplanes[
	int16_t front;     // If > 0,  front = index of Front child node
					   // else,   ~front = index of child leaf
	int16_t back;      // If > 0,   back = index of Back child node
					   // else,    ~back = index of child leaf
	BoundingBoxS box;  // Bounding box of node and all childs
	uint16_t face_id;  // Index of first Polygons in the node
	uint16_t face_num; // Number of faces in the node
};

struct Leaf
{
	int32_t type;          // Special type of leaf
	int32_t visibility_id; // Beginning of visibility lists
						   //     must be -1 or in [0,numvislist[
	BoundingBoxS bound;    // Bounding box of the leaf
	uint16_t listface_id;  // First item of the list of faces
						   //     must be in [0,numlfaces[
	uint16_t listface_num; // Number of faces in the leaf
	uint8_t sndwater;      // level of the four ambient sounds:
	uint8_t sndsky;        //   0    is no sound
	uint8_t sndslime;      //   0xFF is maximum volume
	uint8_t sndlava;       //
};

// uint16_t listface[numlface];   // each uint16_t is the index of a Face

// int32_t listedge[numlstedge];

// uint8_t vislist[numvislist];    // RLE encoded bit array

struct Clipnode
{
	uint32_t planenum; // The plane which splits the node
	int16_t front;     // If positive, id of Front child node
					   // If -2, the Front part is inside the model
					   // If -1, the Front part is outside the model
	int16_t back;      // If positive, id of Back child node
					   // If -2, the Back part is inside the model
					   // If -1, the Back part is outside the model
};

// uint8_t lightmap[numlightmap]; // value 0:dark 255:bright

// uint8_t light[width*height];
#pragma pack(pop)

static Entity
ReadEntity(std::istream& stream)
{
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
			std::string tag, tagValue;
			stream >> std::quoted(tag) >> std::quoted(tagValue);
			entity.tags[tag] = tagValue;
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

struct BSP_File
{
	std::istream& bsp_file;
	Header header;

	BSP_File(std::ifstream& _file) : bsp_file(_file)
	{
		header = ReadT<Header>(bsp_file);
	}

	template<typename T>
	T
	_read(Dir_Entry dir, size_t idx)
	{
		assert(idx < dir.size / sizeof(T));
		bsp_file.clear(0);
		bsp_file.seekg(dir.offset);
		return ReadT<T>(bsp_file, idx);
	}

	std::vector<Entity>
	entities()
	{
		bsp_file.clear(0);
		bsp_file.seekg(header.entities.offset);

		std::vector<Entity> entities{};
		while (bsp_file.tellg() < header.entities.offset + header.entities.size)
			entities.push_back(ReadEntity(bsp_file));

		return entities;
	}

	Plane
	plane(size_t idx)
	{
		return _read<Plane>(header.planes, idx);
	}

	int32_t
	miptex_count()
	{
		bsp_file.clear(0);
		bsp_file.seekg(header.miptex.offset);
		return ReadT<int32_t>(bsp_file);
	}

	Miptex
	miptex(size_t idx)
	{
		int32_t count = miptex_count();
		int32_t offset = ReadT<int32_t>(bsp_file, idx);

		bsp_file.seekg(header.miptex.offset + offset);
		return ReadT<Miptex>(bsp_file);
	}

	Vector3
	vertex(size_t idx)
	{
		return _read<Vector3>(header.vertices, idx);
	}

	Node
	node(size_t idx)
	{
		return _read<Node>(header.nodes, idx);
	}

	TexInfo
	texinfo(size_t idx)
	{
		return _read<TexInfo>(header.texinfos, idx);
	}

	Face
	face(size_t idx)
	{
		return _read<Face>(header.faces, idx);
	}

	Leaf
	leaf(size_t idx)
	{
		return _read<Leaf>(header.leaves, idx);
	}

	uint16_t
	listface(size_t idx)
	{
		return _read<uint16_t>(header.listfaces, idx);
	}

	Edge
	edge(size_t idx)
	{
		return _read<Edge>(header.edges, idx);
	}

	int32_t
	listedge(size_t idx)
	{
		return _read<int32_t>(header.listedges, idx);
	}

	BSP_Model
	model(size_t idx)
	{
		return _read<BSP_Model>(header.models, idx);
	}

	std::vector<Color_RGB8>
	miptex_data(size_t idx, uint8_t miplevel)
	{
		Miptex mptx = miptex(idx);

		uint32_t width = mptx.width >> miplevel;
		uint32_t height = mptx.height >> miplevel;

		bsp_file.seekg(-sizeof(Miptex) + mptx.offset[miplevel], std::ios::cur);

		std::vector<uint8_t> palette_indices(width * height);
		bsp_file.read((char*)palette_indices.data(), width * height);

		std::vector<Color_RGB8> color_data;
		std::transform(palette_indices.begin(), palette_indices.end(), std::back_inserter(color_data), palette);
		return color_data;
	}
};

Vector3
VerticesNormal(Vector3 a, Vector3 b, Vector3 c)
{
	Vector3 ba = Vector3Subtract(b, a);
	Vector3 ca = Vector3Subtract(c, a);
	return Vector3Normalize(Vector3CrossProduct(ba, ca));
}

Mesh
GenMeshFaces(BSP_File& map, std::span<const Face> faces)
{
	static_assert(sizeof(Vector3) == 3 * sizeof(float));
	static_assert(sizeof(Vector2) == 2 * sizeof(float));
	std::vector<Vector3> vertices{};
	std::vector<Vector2> texcoords{};
	std::vector<Vector3> normals{};

	for (const Face& face : faces)
	{
		TexInfo texinfo = map.texinfo(face.texinfo_id);
		Miptex miptex = map.miptex(texinfo.miptex_id);

		std::vector<Vector3> face_vertices{};
		std::vector<Vector2> face_texcoords{};

		for (size_t i = 0; i < face.ledge_num; ++i)
		{
			int16_t ledge = map.listedge(face.ledge_id + i);
			Edge edge = map.edge(labs(ledge));

			Vector3 vertex = map.vertex(ledge >= 0 ? edge.vs : edge.ve);
			face_vertices.push_back(vertex);

			Vector2 uv{
				.x = (Vector3DotProduct(vertex, texinfo.u_axis) + texinfo.u_offset) / miptex.width,
				.y = (Vector3DotProduct(vertex, texinfo.v_axis) + texinfo.v_offset) / miptex.height,
			};
			face_texcoords.push_back(uv);
		}
		assert(face_vertices.empty() == false);

		for (size_t i = face_vertices.size() - 2; i > 0; --i)
		{
			vertices.push_back(face_vertices.back());
			vertices.push_back(face_vertices[i]);
			vertices.push_back(face_vertices[i - 1]);

			texcoords.push_back(face_texcoords.back());
			texcoords.push_back(face_texcoords[i]);
			texcoords.push_back(face_texcoords[i - 1]);

			Vector3 normal = VerticesNormal(face_vertices.back(), face_vertices[i], face_vertices[i - 1]);
			for (size_t v = 0; v < 3; ++v)
				normals.push_back(normal);
		}
	}
	
	Mesh mesh{};
	mesh.vertexCount = vertices.size();
	mesh.vertices = (float*)vertices.data();
	mesh.texcoords = (float*)texcoords.data();
	mesh.normals = (float*)normals.data();
	UploadMesh(&mesh, false);

	// So the free functions don't complain later on
	mesh.vertices = (float*)malloc(1);
	mesh.texcoords = (float*)malloc(1);
	mesh.normals = (float*)malloc(1);
	return mesh;
}

std::vector<Model>
LoadModelsFromBSPFile(const std::filesystem::path& path)
{
	std::ifstream bsp_file{path, std::ios::binary};
	BSP_File map{bsp_file};

	Node bsp_root = map.node(map.model(0).bsp_node_id);
	std::vector<Node> nodes{bsp_root};
	std::set<size_t> leaves{};

	while (nodes.empty() == false)
	{
		Node node = nodes.back();
		nodes.pop_back();

		for (int16_t n : {node.front, node.back})
		{
			if (n > 0)
				nodes.push_back(map.node(n));
			else
			{
				size_t leaf_id = ~n;
				leaves.insert(leaf_id);
			}
		}
	}
	
	std::unordered_map<std::string, Texture> texture_name_to_object{};
	std::unordered_map<std::string, std::vector<Face>> texture_name_to_face_list{}; // Group faces by texture to reduce draw calls

	for (size_t leaf_id : leaves)
	{
		Leaf leaf = map.leaf(leaf_id);
		for (size_t i = 0; i < leaf.listface_num; i++)
		{
			uint16_t face_id = map.listface(leaf.listface_id + i);
			Face face = map.face(face_id);
			
			TexInfo texinfo = map.texinfo(face.texinfo_id);
			Miptex miptex = map.miptex(texinfo.miptex_id);
			
			std::string texname = miptex.name;
			texture_name_to_face_list[texname].push_back(face);
			
			if (texture_name_to_object.contains(texname) == false)
			{
				std::vector<Color_RGB8> color_data = map.miptex_data(texinfo.miptex_id, 0);
				Image texture_image = {
					.data = color_data.data(),
					.width = (int)miptex.width,
					.height = (int)miptex.height,
					.mipmaps = 1,
					.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8,
				};

				texture_name_to_object[texname] = LoadTextureFromImage(texture_image);
			}
		}
	}

	std::vector<Model> models{};
	for (auto& [texname, faces] : texture_name_to_face_list)
	{
		Mesh mesh = GenMeshFaces(map, faces);
		Model model = LoadModelFromMesh(mesh);
		model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture_name_to_object.at(texname);
		models.push_back(model);
	}
	return models;
}