#include "objloader.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define likely(x) __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)

#define IS_SPACE(c) ((c) == ' ' || (c) == '\t')
#define IS_NEW_LINE(c) ((c) == '\r' || (c) == '\n')
#define ATOI_CHARACTERS "+-0123456789"
#define ATOF_CHARACTERS ATOI_CHARACTERS "."

static inline void skipSpace(char **token) {
	char c;
	while ((c = **token) == ' ' || c == '\t') ++*token;
}

static inline void skipWhitespace(char **token) {
	char c;
	while ((c = **token) == ' ' || c == '\n' || c == '\r' || c == '\t') ++*token;
}

static float parseFloat(char **token) {
	skipSpace(token);
	float f = (float) atof(*token);
	*token += strspn(*token, ATOF_CHARACTERS);
	return f;
}

static unsigned fixIndex(unsigned i, unsigned size) {
	// assert(i != 0 && "Invalid vertex index.");
	return i > 0 ? i - 1 : /* negative values = relative */ size + i;
}

static char *parseText(char **token, int flags) {
	char * const start = *token;
	char c;
	while (!(IS_SPACE(c = **token) || IS_NEW_LINE(c) || c == '\0')) ++*token;
	size_t len = *token - start;
	char *buffer;
	if (flags & OBJ_IN_SITU) {
		buffer = start;
	} else {
		buffer = malloc(len * sizeof(char) + 1); // Allocate enough memory for text
		if (!buffer) return 0;
		memcpy(buffer, start, len); // Copy string from line
	}
	buffer[len] = '\0'; // Null-terminate string
	return buffer;
}

static void skipWhitespaceAndComments(char **token) {
	skipWhitespace(token);
	while (unlikely(**token == '#')) {
		while (**token != '\0' && *++*token != '\n') ;
		skipWhitespace(token);
	}
}

static int parseInt(char **token) {
	int value = 0, sign = 1;
	switch(**token) {
		case '-': sign = -1; // Intentional fallthrough
		case '+': ++*token;
	}
	unsigned u;
	while ((u = **token - '0') < 10) {
		value = value * 10 + u;
		++*token;
	}
	return sign * value;
}

static struct ObjVertexIndex parseTriplet(char **token, unsigned vertexCount, unsigned texcoordCount, unsigned normalCount) {
	struct ObjVertexIndex vi;
	// Initialize to invalid values
	vi.texcoordIndex = -1;
	vi.normalIndex = -1;

	vi.vertexIndex = fixIndex(parseInt(token), vertexCount);
	if (**token == '/') {
		*++*token;
		if (**token != '/') {
			vi.texcoordIndex = fixIndex(parseInt(token), texcoordCount);
		}
		if (**token == '/') {
			*++*token;
			vi.normalIndex = fixIndex(parseInt(token), normalCount);
		}
	}
	return vi;
}

static struct ObjGroup *createGroup(char *name) {
	struct ObjGroup *group = malloc(sizeof(struct ObjGroup));
	if (!group) return 0;
	group->name = name;
	group->material = 0;
	group->next = 0;
	group->numFaces = 0;
	group->indicesSize = 0;
	group->indicesCapacity = 0;
	group->indicesPerFaceCapacity = 0;
	if (!(group->indices = malloc(sizeof(struct ObjVertexIndex) * (group->indicesCapacity = 2)))) {
		free(group);
	}
	if (!(group->indicesPerFace = malloc(sizeof(unsigned) * (group->indicesPerFaceCapacity = 2)))) {
		free(group->indices);
		free(group);
	}
	return group;
}

struct ObjModel *objLoadModel(char *data, int flags) {
	struct ObjModel *model = calloc(1, sizeof(struct ObjModel));
	if (!model) return 0;
	model->flags = flags;
	unsigned int verticesCapacity = 3, uvsCapacity = 2, normalsCapacity = 3;
	if(!(model->vertices = malloc(sizeof(float) * verticesCapacity))) goto error;
	if(!(model->texcoords = malloc(sizeof(float) * uvsCapacity))) goto error;
	if(!(model->normals = malloc(sizeof(float) * normalsCapacity))) goto error;
	const int triangulate = flags & OBJ_TRIANGULATE;

	char *currentGroupName = 0; // The name of the current group
	struct ObjGroup *currentGroup;
	if(!(currentGroup = createGroup(0))) goto error;
	model->firstGroup = currentGroup;

	char *token = data;
	while ('\0' != *token) {
		skipWhitespaceAndComments(&token);
		if ('v' == *token) {
			unsigned int *size, *capacity, hasZ = 1;
			float **list;
			if (IS_SPACE(token[1])) { // Position
				size = &model->vertexCount;
				capacity = &verticesCapacity;
				list = &model->vertices;
			} else if ('t' == token[1]) { // Texture coordinate
				size = &model->uvCount;
				capacity = &uvsCapacity;
				list = &model->texcoords;
				hasZ = 0;
			} else if ('n' == token[1]) { // Normal
				size = &model->normalCount;
				capacity = &normalsCapacity;
				list = &model->normals;
			}
			token += 2; // Skip token ('v' and either ' ', 't' or 'n')
			if (*size + (hasZ ? 3 : 2) >= *capacity) {
				float *tmp = realloc(*list, sizeof(float) * (*capacity *= 2));
				if (!tmp) goto error;
				*list = tmp;
			}
			(*list)[(*size)++] = parseFloat(&token); // X
			(*list)[(*size)++] = parseFloat(&token); // Y
			if (hasZ) (*list)[(*size)++] = parseFloat(&token); // Z
		} else if ('f' == *token) {
			token += 2; // Skip the characters 'f '
			skipSpace(&token);

			for (unsigned int i = 0;;) {
				if (currentGroup->indicesSize + 3 > currentGroup->indicesCapacity) {
					struct ObjVertexIndex *tmp = realloc(currentGroup->indices, sizeof(struct ObjVertexIndex) * (currentGroup->indicesCapacity *= 2));
					if (!tmp) goto error;
					currentGroup->indices = tmp;
				}

				if (triangulate && i >= 3) {
					// Create a triangle fan
					struct ObjVertexIndex first = currentGroup->indices[currentGroup->indicesSize - i], second = currentGroup->indices[currentGroup->indicesSize - 1];
					currentGroup->indices[currentGroup->indicesSize++] = first;
					currentGroup->indices[currentGroup->indicesSize++] = second;
					i += 2;
				}

				// Parse a triplet
				currentGroup->indices[currentGroup->indicesSize++] = parseTriplet(&token, model->vertexCount, model->uvCount, model->normalCount);
				++i;

				skipSpace(&token);
				if (IS_NEW_LINE(*token) || triangulate && i % 3 == 0) {
					if (currentGroup->numFaces + 1 > currentGroup->indicesPerFaceCapacity) {
						unsigned *tmp = realloc(currentGroup->indicesPerFace, sizeof(unsigned) * (currentGroup->indicesPerFaceCapacity *= 2));
						if (!tmp) goto error;
						currentGroup->indicesPerFace = tmp;
					}
					currentGroup->indicesPerFace[currentGroup->numFaces++] = triangulate ? 3 : i;
					if (IS_NEW_LINE(*token)) break;
				}
			}
		} else if ('g' == *token) {
			token += 2; // Skip the 2 chars in "g "
			skipWhitespace(&token);
			char *groupName = parseText(&token, flags);
			if (!groupName) goto error;
			if (currentGroup->numFaces == 0) {
				// If the last group is empty, just overwrite the name
				free(currentGroup->name);
				currentGroupName = currentGroup->name = groupName;
			} else {
				// Append a new group to the tail
				struct ObjGroup *oldGroup = currentGroup;
				if (!(currentGroup = createGroup(groupName))) goto error;
				while (oldGroup->next) oldGroup = oldGroup->next;
				oldGroup->next = currentGroup;
			}
		} else if (0 == strncmp("usemtl", token, strlen("usemtl"))) {
			token += 7; // Skip the 7 chars in "usemtl "
			char *material = parseText(&token, flags); // Parse the name of the material
			if (!material) goto error;

			int existingGroup = 0;
			struct ObjGroup *group = model->firstGroup;
			char *lastGroupName, *lastMaterial;
			do {
				if (group->name) lastGroupName = group->name;
				if (group->material) lastMaterial = group->material;
				if ((!currentGroupName || (lastGroupName && strcmp(lastGroupName, currentGroupName) == 0)) && lastMaterial && strcmp(lastMaterial, material) == 0) {
					currentGroup = group;
					existingGroup = 1;
					break;
				}
			} while (group = group->next);
			if (!existingGroup) {
				// If the current group already has faces that doesn't use the new material we need to differentiate them
				if (currentGroup->numFaces > 0) {
					// Create a new group after the current group
					struct ObjGroup *oldGroup = currentGroup;
					if (!(currentGroup = createGroup(0))) {
						free(material);
						goto error;
					}
					oldGroup->next = currentGroup;
				}
			}
			currentGroup->material = material; // Set the material
		} else if (0 == strncmp("mtllib", token, strlen("mtllib"))) {
			// TODO load multiple mtllibs as per definition: mtllib filename1 filename2 . . .
			token += 7; // Skip the 7 chars in "mtllib "
			char *filename = parseText(&token, flags);
			if (!filename) goto error;
			char **tmp = realloc(model->materialLibraries, sizeof(char *) * (++model->numMaterialLibraries));
			if (!tmp) {
				free(filename);
				goto error;
			}
			(model->materialLibraries = tmp)[model->numMaterialLibraries] = filename;
		}
		token += strcspn(token, "\n") + 1; // Skip to next line
	}

	return model;
error:
	free(model->vertices);
	free(model->texcoords);
	free(model->normals);
	struct ObjGroup *group = model->firstGroup;
	while (group) {
		struct ObjGroup *next = group->next;
		free(group->name);
		free(group->material);
		free(group->indices);
		free(group->indicesPerFace);
		free(group);
		group = next;
	}
	for (unsigned int i = 0; i < model->numMaterialLibraries; ++i) {
		free(model->materialLibraries[i]);
	}
	free(model->materialLibraries);
	free(model);
	return 0;
}

void objDestroyModel(struct ObjModel *model) {
	struct ObjGroup *group = model->firstGroup;
	do {
		struct ObjGroup *next = group->next;
		free(group->name);
		free(group->material);
		free(group->indices);
		free(group->indicesPerFace);
		free(group);
		group = next;
	} while (group);
	for (unsigned int i = 0; i < model->numMaterialLibraries; ++i) {
		free(model->materialLibraries[i]);
	}
	free(model->materialLibraries);
	free(model->vertices);
	free(model->texcoords);
	free(model->normals);
	free(model);
}

// TODO
struct MtlMaterial *loadMtl(char *data, unsigned int *numMaterials, int flags) {
	unsigned int size = 0, capacity = 2;
	struct MtlMaterial *materials = malloc(capacity * sizeof(struct MtlMaterial)), *current; // Current material from last newmtl
	if (!materials) return 0;

	char *token = data;
	for (;;) {
		if (strncmp("newmtl", token, strlen("newmtl")) == 0) {
			token += 7; // Skip the 7 characters in "newmtl "
			if (size + 1 >= capacity) {
				struct MtlMaterial *tmp = realloc(materials, (capacity *= 2) * sizeof(struct MtlMaterial));
				if (tmp == 0) {
					free(materials);
					return 0;
				}
				materials = tmp;
			}
			current = materials + size++;
			current->name = parseText(&token, flags);
			// TODO define defaults for rest
			current->opacity = 1.f;
			current->ambientTexture = 0;
		} else if (*token == 'K' && (token[1] == 'a' || token[1] == 'd' || token[1] == 's')) { // diffuse or specular
			float *color = token[1] == 'a' ? current->ambient : (token[1] == 'd' ? current->diffuse : current->specular);
			token += 3; // Skip the characters 'K' and 'a'/'d'/'s' and the space
			color[0] = parseFloat(&token);
			color[1] = parseFloat(&token);
			color[2] = parseFloat(&token);
		} else if ((token[0] == 'T' && token[1] == 'r') || *token == 'd') {
			token += 2; // Skip the characters 'Tr' or 'd '
			current->opacity = parseFloat(&token);
		} else if (token[0] == 'n' && token[1] == 's') {
			token += 3; // Skip the characters 'ns '
			current->shininess = parseFloat(&token);
		} else if (strncmp("map_Kd", token, strlen("map_Kd")) == 0) {
			token += 7; // Skip the 7 characters "map_Kd "
			current->ambientTexture = parseText(&token, flags);
			token += 0;
		} else if (*token == ' ' || *token == '\n' || *token == '\t' || *token == 'r') {
			token++; // Skip spaces or empty lines at beginning of line
			continue;
		} else if ('\0' == *token) break;
		token += strcspn(token, "\n") + 1; // Skip to next line
	}

	*numMaterials = size;
	return materials;
}

void destroyMtlMaterial(struct MtlMaterial material) {
	free(material.name);
	if (material.ambientTexture != 0) free(material.ambientTexture);
}
