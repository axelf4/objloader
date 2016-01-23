/** An OBJ model loader.
	Supports polygonal geometry and face elements.
	@file objloader.h */

#ifndef OBJLOADER_H
#define OBJLOADER_H

#ifdef __cplusplus
extern "C" {
#endif

	enum {
		/** Generates indices and populates \c obj_model_part.indices. */
		OBJ_INDICES = 0x1,
		/** Merge mesh parts that share the same material, effectively reducing the total number of meshes. */
		OBJ_OPTIMIZE_MESHES = 0x2,
		/** Use existing found indices, when ::OBJ_INDICES is specified. */
		OBJ_JOIN_IDENTICAL_VERTICES = 0x4
	};

	/** A MTL material definition. */
	struct mtl_material {
		char *name, /**< The name of the material. */
			*ambientTexture; /**< The path to the diffuse texture. */

		float ambient[3], /**< The ambient color, declared using *Ka r g b*, where the RGB values range between 0-1. */
			diffuse[3], /**< The diffuse color, declared using *Ka r g b*, where the RGB values range between 0-1. */
			specular[3], /**< The specular color, declared using *Ka r g b*, where the RGB values range between 0-1. */

			shininess, /**< The specular shininess, declared with *Ns #*, where # ranges between 0-1000. */
			opacity; /**< The transparency, declared with *Tr* or *d alpha*, ranges 0-1, where 1 is the default and not transparent at all. */
	};

	/** A collection of elements in an OBJ model. */
	struct obj_group {
		char *name, /**< The name of the group. */
			 *materialName; /**< The material name for this group, or if @c 0 the same material as the last group. */
		unsigned int numFaces, /**< This group's number of triangles. */
					 facesSize, /**< The number of elements in the #faces array. */
					 facesCapacity, /**< The allocated size of the #faces array. */
					 *faces, /**< Array of triangles. Interleaved with position, (optional) uv, (optional) normal, ... */
					 materialIndex; /**< Index of this group's material in obj_model#materials. */
	};

	/** An OBJ model. */
	struct obj_model {
		unsigned int numGroups, /**< The number of groups. */
					 numMaterialLibraries, /**< The number of material libraries. */
					 hasUVs, /**< Whether this model has texture coordinates. */
					 hasNorms, /**< Whether this model has normals. */
					 vertCount, /**< The number of vertices in #verts. */
					 uvCount, /**< The number of texture coordinates in #uvs. */
					 normCount; /**< The number of normals in #norms. */
		float *verts, /**< Array of the positions. */
			  *uvs, /**< Array of the texture coordinates. */
			  *norms; /**< Array of the normals. */
		struct obj_group *groups; /**< Array of groups. */
		char **materialLibraries; /**< An array of relative filenames to material libraries. */
	};

	/** Loads an OBJ model.
		@param data The data of the model file, must be null-terminated
		@param flags Additional flags use when loading
		@return The OBJ model */
	struct obj_model *obj_load_model(const char *data, int flags);

	/** Destroys an OBJ model.
		@param model The model to free */
	void obj_destroy_model(struct obj_model *model);

	/** Loads a MTL material definition from a string.
		@param data The contents of the material file
		@param numMaterials A pointer to a integer which will be set to the number of materials
		@return An array of materials */
	struct mtl_material *load_mtl(const char *data, unsigned int *numMaterials);

	/** Destroys a MTL material.
		@param material The material to destroy */
	void destroy_mtl_material(struct mtl_material material);

	void obj_get_vertices(struct obj_model *model, struct obj_group *group, unsigned int *vertexCount, unsigned int *indexCount, float **vertices, unsigned int **indices);

#ifdef __cplusplus
}
#endif

#endif
