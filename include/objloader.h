/** An OBJ model loader.
	Supports polygonal geometry and face elements.
	@file objloader.h */

#ifndef OBJLOADER_H
#define OBJLOADER_H

#ifdef __cplusplus
extern "C" {
#endif

	enum {
		/**
		 * Flag to triangulate all shapes.
		 * Faces are assumed to be coplanar and convex.
		 */
		OBJ_TRIANGULATE = 0x1,
		/** Merge mesh parts that share the same material, effectively reducing the total number of meshes. */
		OBJ_OPTIMIZE_MESHES = 0x2,
		/** Flag to enable potentially destructive in-situ string parsing. */
		OBJ_IN_SITU = 0x4,
	};

	/** A MTL material definition. */
	struct MtlMaterial {
		char *name, /**< The name of the material. */
			*ambientTexture; /**< The path to the diffuse texture. */

		float ambient[3], /**< The ambient color, declared using *Ka r g b*, where the RGB values range between 0-1. */
			diffuse[3], /**< The diffuse color, declared using *Ka r g b*, where the RGB values range between 0-1. */
			specular[3], /**< The specular color, declared using *Ka r g b*, where the RGB values range between 0-1. */

			shininess, /**< The specular shininess, declared with *Ns #*, where # ranges between 0-1000. */
			opacity; /**< The transparency, declared with *Tr* or *d alpha*, ranges 0-1, where 1 is the default and not transparent at all. */
	};

	/** The indices to the data of a vertex. */
	struct ObjVertexIndex {
		/** The index of the vertex. */
		int vertexIndex,
			/** The index of the texture coordinate. */
			texcoordIndex,
			/** The index of the normal. */
			normalIndex;
	};

	/** A collection of elements in an OBJ model. */
	struct ObjGroup {
		/** The name of the group, or @c 0 if it's the same as the last group's. */
		char *name,
			 /** The name of this group's material, or @c 0 if it's the same as the last group's. */
			 *material;
		/** The next group. */
		struct ObjGroup *next;
		/** Array of indices. */
		struct ObjVertexIndex *indices;
		/** The number of indices. */
		unsigned int indicesSize,
					 /** The capacity of the indices array. */
					 indicesCapacity,
					 // TODO rename to indicesPerFace
					 /** Array of the number of indices per each face. */
					 *indicesPerFace,
					 /** The number of faces. */
					 numFaces,
					 indicesPerFaceCapacity;
	};

	/** An OBJ model. */
	struct ObjModel {
		unsigned int // TODO rename these
					 vertexCount, /**< The number of vertices in #verts. */
					 uvCount, /**< The number of texture coordinates in #uvs. */
					 normalCount, /**< The number of normals in #norms. */
					 numMaterialLibraries; /**< The number of material libraries. */
		float *vertices, /**< Array of positions. */
			  *texcoords, /**< Array of texture coordinates. */
			  *normals; /**< Array of normals. */
		/** The first group in linked list. */
		struct ObjGroup *firstGroup;
		/** Array of filenames to material libraries, relative to the OBJ file. */
		char **materialLibraries;
		/** The flags used when parsing the model. */
		int flags;
	};

	/** Loads an OBJ model.
		@param data The data of the model file, must be null-terminated
		@param flags Additional flags use when loading
		@return The OBJ model */
	struct ObjModel *objLoadModel(char *data, int flags);

	/** Destroys an OBJ model.
		@param model The model to free */
	void objDestroyModel(struct ObjModel *model);

	/** Loads a MTL material definition from a string.
		@param data The contents of the material file
		@param numMaterials A pointer to a integer which will be set to the number of materials
		@return An array of materials */
	struct MtlMaterial *loadMtl(char *data, unsigned int *numMaterials, int flags);

	/** Destroys a MTL material.
		@param material The material to destroy */
	void destroyMtlMaterial(struct MtlMaterial material);

#ifdef __cplusplus
}
#endif

#endif
