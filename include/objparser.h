/**
 * OBJ model parser.
 * Supports polygonal geometry and face elements.
 * @file objparser.h
 */

#ifndef objparser_H
#define objparser_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

	enum {
		/**
		 * Flag to triangulate all shapes.
		 * Faces are assumed to be coplanar and convex.
		 */
		OBJ_TRIANGULATE = 0x1,
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
		/** The index of the vertex, or @c -1. */
		int vertexIndex,
			/** The index of the texture coordinate, or @c -1. */
			texcoordIndex,
			/** The index of the normal, or @c -1. */
			normalIndex;
	};

	typedef struct ObjParserContext {
		/** User data. */
		void *prv;
		/** Callback for adding a vertex position. */
		void (*addVertex)(void *prv, float x, float y, float z, float w);
		/** Callback for adding a texture coordinate. */
		void (*addTexcoord)(void *prv, float x, float y, float z);
		/** Callback for adding a normal. */
		void (*addNormal)(void *prv, float x, float y, float z);
		/** Callback for adding a face. */
		void (*addFace)(void *prv, int numVertices, struct ObjVertexIndex *indices);
		void (*addGroup)(void *prv, int numNames, char **names);
		void (*mtllib)(void *prv, char *path);
		void (*usemtl)(void *prv, char *name);
		/** Allocates memory. */
		void *(*malloc)(size_t size);
		/** Frees memory. */
		void (*free)(void *ptr);
		/** Flags to use when parsing. */
		int flags;
	} objparserContext;

	/**
	 * Parses an OBJ model from a string.
	 * @param context The callbacks for parsing
	 * @param buffer The string to parse from
	 * @return Zero for success.
	 */
	int objParse(struct ObjParserContext *context, char *buffer);

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
