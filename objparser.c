#include "objparser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define likely(x) __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)

#define IS_SPACE(c) ((c) == ' ' || (c) == '\t')
#define IS_NEW_LINE(c) ((c) == '\r' || (c) == '\n')
#define ATOF_CHARACTERS "+-0123456789."

static inline void skipSpace(char **token) {
	char c;
	while ((c = **token) == ' ' || c == '\t') ++*token;
}

static inline void skipWhitespace(char **token) {
	char c;
	while ((c = **token) == ' ' || c == '\n' || c == '\r' || c == '\t') ++*token;
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

static float parseFloat(char **token) {
	skipSpace(token);
	float f = (float) atof(*token);
	*token += strspn(*token, ATOF_CHARACTERS);
	return f;
}

static int parseFloat2(char **token, float *value) {
	float sign = 1;
	switch (**token) {
		case '-': sign = -1; // Intentional fallthrough
		case '+': ++*token;
	}
	switch (**token) {
	}
}

static char *parseText(char **token, int flags) {
	char *buffer, * const start = *token, c;
	while (!(IS_SPACE(c = **token) || IS_NEW_LINE(c) || c == '\0')) ++*token;
	size_t len = *token - start;
	if (flags & OBJ_IN_SITU) {
		buffer = start;
		++*token; // Skip the null terminator
	} else {
		buffer = malloc(len * sizeof(char) + 1); // Allocate enough memory for text
		if (!buffer) return 0;
		memcpy(buffer, start, len); // Copy string from line
	}
	buffer[len] = '\0'; // Null-terminate string
	return buffer;
}

static char *parseString(struct ObjParserContext *context, char **token) {
	char * const start = *token, c = **token;
	while (!(IS_SPACE(c) || IS_NEW_LINE(c) || c == '\0')) c = *++*token;
	const size_t len = *token - start;
	char *buffer = context->malloc(len * sizeof(char) + 1); // Allocate enough memory for text
	if (!buffer) return 0;
	memcpy(buffer, start, len); // Copy string from line
	buffer[len] = '\0'; // Null-terminate string
	return buffer;
}

static void skipWhitespaceAndComments(char **token) {
	skipWhitespace(token);
	while (unlikely(**token == '#')) {
		while (**token != '\0' && **token != '\n') ++*token;
		skipWhitespace(token);
	}
}

static unsigned fixIndex(unsigned i, unsigned size) {
	return i > 0 ? /* 1-based */ i - 1 : /* negative values = relative */ size + i;
}

static struct ObjVertexIndex parseTriplet(char **token, unsigned vertexCount, unsigned texcoordCount, unsigned normalCount) {
	struct ObjVertexIndex vi;
	vi.vertexIndex = fixIndex(parseInt(token), vertexCount);
	// Initialize to invalid values
	vi.texcoordIndex = -1;
	vi.normalIndex = -1;
	if (**token == '/') {
		++*token;
		if (**token != '/') {
			vi.texcoordIndex = fixIndex(parseInt(token), texcoordCount);
		}
		if (**token == '/') {
			++*token;
			vi.normalIndex = fixIndex(parseInt(token), normalCount);
		}
	}
	return vi;
}

int objParse(struct ObjParserContext *context, char *buffer) {
	const int triangulate = context->flags & OBJ_TRIANGULATE;
	unsigned int vertexCount = 0, texcoordCount = 0, normalCount = 0;
	struct ObjVertexIndex indexBuffer[256];

	char *token = buffer;
	while ('\0' != *token) {
		skipWhitespaceAndComments(&token);
		if ('v' == *token) {
			// TODO optional values
			if (IS_SPACE(token[1])) {
				token += 2;
				float x, y, z;
				x = parseFloat(&token);
				y = parseFloat(&token);
				z = parseFloat(&token);
				++vertexCount;
				context->addVertex(context->prv, x, y, z, 1.0);
			} else if ('t' == token[1]) {
				token += 2;
				float x, y;
				x = parseFloat(&token);
				y = parseFloat(&token);
				++texcoordCount;
				context->addTexcoord(context->prv, x, y, 0);
			} else if ('n' == token[1]) {
				token += 2;
				float x, y, z;
				x = parseFloat(&token);
				y = parseFloat(&token);
				z = parseFloat(&token);
				++normalCount;
				context->addNormal(context->prv, x, y, z);
			}
		} else if ('f' == *token) {
			token += 2; // Skip the characters 'f '
			skipSpace(&token);
			for (int i = 0;;) {
				indexBuffer[i] = parseTriplet(&token, vertexCount, texcoordCount, normalCount); // Parse a triplet
				skipSpace(&token);
				if (IS_NEW_LINE(*token) || triangulate && i >= 2) {
					context->addFace(context->prv, i + 1, indexBuffer);
					if (IS_NEW_LINE(*token)) break;
					indexBuffer[1] = indexBuffer[2]; // Construct a triangle fan
				} else ++i;
			}
		} else if ('g' == *token) {
			token += 2; // Skip the 2 chars in "g "
			// TODO parse multiple groups
			skipWhitespace(&token);
			char *groupName = parseText(&token, context->flags);
			if (!groupName) goto error;
			context->addGroup(context->prv, 1, &groupName);
			if (!(context->flags & OBJ_IN_SITU)) context->free(groupName);
		} else if (0 == strncmp("usemtl", token, strlen("usemtl"))) {
			token += 7; // Skip the 7 chars in "usemtl "
			char *material = parseText(&token, context->flags); // Parse the name of the material
			if (!material) goto error;
			context->usemtl(context->prv, material);
			if (!(context->flags & OBJ_IN_SITU)) context->free(material);
		} else if (0 == strncmp("mtllib", token, strlen("mtllib"))) {
			// TODO load multiple mtllibs as per definition: mtllib filename1 filename2 . . .
			token += 7; // Skip the 7 chars in "mtllib "
			char *filename = parseText(&token, context->flags);
			if (!filename) goto error;
			context->mtllib(context->prv, filename);
			if (!(context->flags & OBJ_IN_SITU)) context->free(filename);
		}
		token += strcspn(token, "\n") + 1; // Skip to next line
	}
	return 0;
error:
	return 1;
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
