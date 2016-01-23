#include "objloader.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h> // isspace
#include <assert.h>
#include <math.h>

#define SPACE " \t\n\v\f\r"
#define ATOI_CHARACTERS "+-0123456789"
#define ATOF_CHARACTERS ATOI_CHARACTERS "."

static int isNewLine(const char c) {
	return (c == '\r') || (c == '\n') || (c == '\0');
}

static float parseFloat(const char **token) {
	*token += strspn(*token, SPACE);
	float f = (float) atof(*token);
	*token += strspn(*token, ATOF_CHARACTERS);
	return f;
}

static unsigned int getIndex(const char **token, unsigned int size) {
	int idx = atoi(*token);
	*token += strspn(*token += strspn(*token, SPACE), ATOI_CHARACTERS);
	return (idx > 0 ? idx - 1 : (idx < 0 ? /* negative value = relative */ size + idx : 0));
}

static char *parseText(const char **token) {
	unsigned int len = strspn(*token, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.");
	char *buffer = malloc(len * sizeof(char) + 1); // Allocate enough memory for text
	if (!buffer) return 0;
	memcpy(buffer, *token, len); // Copy string from line
	buffer[len] = '\0'; // Manually null-terminate string
	*token += len;
	return buffer;
}

static struct obj_group *create_group(char *name, unsigned int *numGroups, struct obj_group **groups) {
	struct obj_group *tmp = realloc(*groups, sizeof(struct obj_group) * (*numGroups + 1));
	if (!tmp) return 0;
	*groups = tmp;

	struct obj_group *group = *groups + *numGroups;
	group->name = name;
	group->materialName = 0;
	group->materialIndex = 0;
	group->numFaces = 0;
	group->facesSize = 0;
	if ((group->faces = malloc(sizeof(unsigned int) * (group->facesCapacity = 3))) == 0) {
		free(name);
		return 0;
	}
	++*numGroups;
	return group;
}

struct obj_model *obj_load_model(const char *data, int flags) {
	struct obj_model *model = calloc(1, sizeof(struct obj_model));
	if (!model) return 0;
	unsigned int vertsCapacity = 2, uvsCapacity = 2, normsCapacity = 2;
	char *defaultGroupName = 0;
	struct obj_group *currentGroup;
	if(!(model->verts = malloc(sizeof(float) * vertsCapacity))) goto error;
	if(!(model->uvs = malloc(sizeof(float) * vertsCapacity))) goto error;
	if(!(model->norms = malloc(sizeof(float) * normsCapacity))) goto error;
	if(!(defaultGroupName = malloc(sizeof "default"))) goto error; // Allocate string on heap to be able to free it like the rest
	strcpy(defaultGroupName, "default");
	if(!(currentGroup = create_group(defaultGroupName, &model->numGroups, &model->groups))) goto error;

	const char *tok = data;
	while ('\0' != *tok) {
		if ('v' == *tok) {
			unsigned int *size, *capacity, hasZ = 1;
			float **list, x, y, z;
			if (' ' == *++tok) { // Position
				size = &model->vertCount;
				capacity = &vertsCapacity;
				list = &model->verts;
			} else if ('t' == *tok) { // Texture coordinate
				size = &model->uvCount;
				capacity = &uvsCapacity;
				list = &model->uvs;
				hasZ = 0;
			} else if ('n' == *tok) { // Normal
				size = &model->normCount;
				capacity = &normsCapacity;
				list = &model->norms;
			}
			tok++; // Skip token (either ' ', 't' or 'n')
			if (*size + (hasZ ? 3 : 2) >= *capacity) {
				float *tmp = realloc(*list, sizeof(float) * (*capacity *= 2));
				if (!tmp) goto error;
				*list = tmp;
			}
			x = (*list)[(*size)++] = parseFloat(&tok); // X
			y = (*list)[(*size)++] = parseFloat(&tok); // Y
			if (hasZ) z = (*list)[(*size)++] = parseFloat(&tok); // Z
		} else if ('f' == *tok) {
			tok += 2; // Skip the characters 'f '
			unsigned int i, initialFacesSize, *facesSize = &currentGroup->facesSize, *facesCapacity = &currentGroup->facesCapacity, *numFaces = &currentGroup->numFaces, **faces = &currentGroup->faces;
			initialFacesSize = *facesSize;
			for (i = 0; !isNewLine(*tok); i++) {
				if (*facesSize + (1 + model->hasUVs + model->hasNorms) * (i >= 3 ? 3 : 1) >= *facesCapacity) {
					unsigned int *tmp = realloc(*faces, (*facesCapacity *= 2) * sizeof(unsigned int));
					if (!tmp) goto error;
					*faces = tmp;
				}

				if (i >= 3) {
					// More points than a triangle: triangulate it
					unsigned int lastFacesSize = *facesSize;

					(*faces)[(*facesSize)++] = (*faces)[initialFacesSize];
					if (model->hasUVs) (*faces)[(*facesSize)++] = (*faces)[initialFacesSize + 1];
					if (model->hasNorms) (*faces)[(*facesSize)++] = (*faces)[initialFacesSize + 1 + model->hasUVs];

					(*faces)[(*facesSize)++] = (*faces)[lastFacesSize - (1 + model->hasUVs + model->hasNorms)];
					if (model->hasUVs) (*faces)[(*facesSize)++] = (*faces)[lastFacesSize - (model->hasUVs + model->hasNorms)];
					if (model->hasNorms) (*faces)[(*facesSize)++] = (*faces)[lastFacesSize - model->hasNorms];

					currentGroup->numFaces += 2;
				}

				unsigned int x, y, z;
				x = (*faces)[(*facesSize)++] = getIndex(&tok, model->vertCount); // Faces always define geometric data
				if (*tok++ == '/' /* && *tok != '/' */) {
					y = (*faces)[(*facesSize)++] = getIndex(&tok, model->uvCount);
					model->hasUVs = 1;
				}
				if (*tok++ == '/') {
					z = (*faces)[(*facesSize)++] = getIndex(&tok, model->normCount);
					model->hasNorms = 1;
				}
				currentGroup->numFaces++;
				tok += strspn(tok, " \t\r"); // Ignore spaces when checking for a newline
			}
			assert(i >= 2 && "Points and lines aren't supported.");
		} else if ('g' == *tok) {
			tok += 2; // Skip the 2 chars in "g "
			if (!(currentGroup = create_group(parseText(&tok), &model->numGroups, &model->groups))) goto error; // Create a new group
		} else if (strncmp("usemtl", tok, strlen("usemtl")) == 0) {
			tok += 7; // Skip the 7 chars in "usemtl "
			char *materialName = parseText(&tok); // Parse the name of the material
			if (!materialName) goto error;
			int existingGroup = 0;
			if (flags & OBJ_OPTIMIZE_MESHES) {
				// If a group using the same material is found: set currentGroup to it
				for (int i = 0; i < model->numGroups; i++) if (model->groups[i].materialName && strcmp(model->groups[i].materialName, materialName) == 0) {
					currentGroup = model->groups + i;
					existingGroup = 1;
					break;
				}
			}
			if (!existingGroup) {
				// If the current group already has faces that doesn't use the new material we need to differentiate them
				if (currentGroup->numFaces > 0) {
					char *groupName = malloc(strlen(currentGroup->name) + 1); // Allocate memory for the name of the new group
					if (!groupName) {
						free(materialName);
						goto error;
					}
					strcpy(groupName, currentGroup->name); // Copy the name of the old group into the new one's
					// Create a new group sharing old one's name and link them
					if (!(currentGroup = create_group(groupName, &model->numGroups, &model->groups))) {
						free(materialName);
						free(groupName);
						goto error;
					}
				}
				currentGroup->materialName = materialName;
			}
		} else if (*tok == ' ' || *tok == '\n' || *tok == '\t' || *tok == 'r') {
			tok++; // Skip spaces or empty lines at beginning of line
			continue;
		} else if ('#' == *tok) {
			tok += strcspn(tok + 1, "\n") + 1; // Skip newlines or comments
		} else if (strncmp("mtllib", tok, strlen("mtllib")) == 0) {
			// TODO load multiple mtllibs as per definition: mtllib filename1 filename2 . . .
			tok += 7; // Skip the 7 chars in "mtllib "
			char *filename = parseText(&tok);
			if (!filename) goto error;

			char **tmp = realloc(model->materialLibraries, (model->numMaterialLibraries + 1) * sizeof(char *));
			if (!tmp) {
				free(filename);
				goto error;
			}
			(model->materialLibraries = tmp)[model->numMaterialLibraries++] = filename;
		}
		tok += strcspn(tok, "\n") + 1; // Skip to next line
	}

	return model;
error:
	free(model->verts);
	free(model->uvs);
	free(model->norms);
	free(defaultGroupName);
	for (unsigned int i = 0; i < model->numGroups; i++) {
		struct obj_group *group = model->groups + i;
		free(group->name);
		free(group->faces);
		free(group->materialName);
	}
	free(model->groups);
	for (unsigned int i = 0; i < model->numMaterialLibraries; i++) free(model->materialLibraries[i]);
	free(model->materialLibraries);
	return 0;
}

void obj_destroy_model(struct obj_model *model) {
	for (unsigned int i = 0; i < model->numGroups; i++) {
		struct obj_group group = model->groups[i];
		free(group.name);
		free(group.faces);
		free(group.materialName);
	}
	free(model->groups);
	for (unsigned int i = 0; i < model->numMaterialLibraries; i++) free(model->materialLibraries[i]);
	free(model->materialLibraries);
	free(model->verts);
	free(model->uvs);
	free(model->norms);
	free(model);
}

struct mtl_material *load_mtl(const char *data, unsigned int *numMaterials) {
	unsigned int size = 0, capacity = 2;
	struct mtl_material *materials = malloc(capacity * sizeof(struct mtl_material)),
						*current; // Current material from last newmtl
	if (!materials) return 0;

	const char *tok = data;
	for (;;) {
		if (strncmp("newmtl", tok, strlen("newmtl")) == 0) {
			tok += 7; // Skip the 7 characters in "newmtl "
			if (size + 1 >= capacity) {
				struct mtl_material *tmp = realloc(materials, (capacity *= 2) * sizeof(struct mtl_material));
				if (tmp == 0) {
					free(materials);
					return 0;
				}
				materials = tmp;
			}
			current = materials + size++;
			current->name = parseText(&tok);
			// TODO define defaults for rest
			current->opacity = 1.f;
			current->ambientTexture = 0;
		} else if (*tok == 'K' && (tok[1] == 'a' || tok[1] == 'd' || tok[1] == 's')) { // diffuse or specular
			float *color = tok[1] == 'a' ? current->ambient : (tok[1] == 'd' ? current->diffuse : current->specular);
			tok += 3; // Skip the characters 'K' and 'a'/'d'/'s' and the space
			color[0] = parseFloat(&tok);
			color[1] = parseFloat(&tok);
			color[2] = parseFloat(&tok);
		} else if ((tok[0] == 'T' && tok[1] == 'r') || *tok == 'd') {
			tok += 2; // Skip the characters 'Tr' or 'd '
			current->opacity = parseFloat(&tok);
		} else if (tok[0] == 'n' && tok[1] == 's') {
			tok += 3; // Skip the characters 'ns '
			current->shininess = parseFloat(&tok);
		} else if (strncmp("map_Kd", tok, strlen("map_Kd")) == 0) {
			tok += 7; // Skip the 7 characters "map_Kd "
			current->ambientTexture = parseText(&tok);
			tok += 0;
		} else if (*tok == ' ' || *tok == '\n' || *tok == '\t' || *tok == 'r') {
			tok++; // Skip spaces or empty lines at beginning of line
			continue;
		} else if ('\0' == *tok) break;
		tok += strcspn(tok, "\n") + 1; // Skip to next line
	}

	*numMaterials = size;
	return materials;
}

void destroy_mtl_material(struct mtl_material material) {
	free(material.name);
	if (material.ambientTexture != 0) free(material.ambientTexture);
}

void obj_get_vertices(struct obj_model *model, struct obj_group *group, unsigned int *vertexCount, unsigned int *indexCount, float **vertices, unsigned int **indices) {
	int hasUVs = model->hasUVs, hasNorms = model->hasNorms;
	int flags = OBJ_INDICES;
	float *verts = model->verts, *uvs = model->uvs, *norms = model->norms;
	*vertices = malloc(sizeof(float) * (*vertexCount = group->numFaces * (3 + (hasUVs ? 2 : 0) + (hasNorms ? 3 : 0))));
	if (!(flags & OBJ_INDICES)) {
		*indexCount = 0;
		*indices = 0;
		for (unsigned int j = 0, vi = 0; j < group->facesSize;) {
			int vertIndex = group->faces[j++] * 3;
			*vertices[vi++] = verts[vertIndex++];
			*vertices[vi++] = verts[vertIndex++];
			*vertices[vi++] = verts[vertIndex];
			if (hasUVs) {
				int uvIndex = group->faces[j++] * 2;
				*vertices[vi++] = uvs[uvIndex++];
				*vertices[vi++] = uvs[uvIndex];
			}
			if (hasNorms) {
				int normIndex = group->faces[j++] * 3;
				*vertices[vi++] = norms[normIndex++];
				*vertices[vi++] = norms[normIndex++];
				*vertices[vi++] = norms[normIndex];
			}
		}
	} else {
		unsigned int vi = 0;
	   	*indices = malloc(sizeof(unsigned int) * (*indexCount = group->numFaces));
		for (unsigned int j = 0, k = 0; j < group->facesSize; k++) {
			unsigned int vertIndex = group->faces[j++] * 3, uvIndex = hasUVs ? group->faces[j++] * 2 : 0, normIndex = hasNorms ? group->faces[j++] * 3 : 0;

			int existing = 0;
			// Iterate through existing indices to find a match
			if (flags & OBJ_JOIN_IDENTICAL_VERTICES) for (unsigned int l = 0; l < j - (1 + hasUVs + hasNorms);) {
				if (vertIndex == group->faces[l++] * 3 && (!hasUVs || uvIndex == group->faces[l++] * 2) && (!hasNorms || normIndex == group->faces[l++] * 3)) {
					existing = 1;
					(*indices)[k] = (*indices)[(l - (1 + hasUVs + hasNorms)) / (1 + hasUVs + hasNorms)];
					break;
				}
			}
			if (!existing) {
				(*indices)[k] = vi / (3 + hasUVs * 2 + hasNorms * 3);
				(*vertices)[vi++] = verts[vertIndex++];
				(*vertices)[vi++] = verts[vertIndex++];
				(*vertices)[vi++] = verts[vertIndex];
				if (hasUVs) {
					(*vertices)[vi++] = uvs[uvIndex++];
					(*vertices)[vi++] = uvs[uvIndex];
				}
				if (hasNorms) {
					(*vertices)[vi++] = norms[normIndex++];
					(*vertices)[vi++] = norms[normIndex++];
					(*vertices)[vi++] = norms[normIndex];
				}
			}
		}
		*vertexCount = vi;
		float *tmp = realloc(*vertices, sizeof(float) * vi); // Reallocate the array to a save some memory
		if (tmp) *vertices= tmp;
	}
}
