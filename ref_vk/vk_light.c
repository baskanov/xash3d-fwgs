#include "vk_light.h"
#include "vk_textures.h"

#include "mod_local.h"
#include "xash3d_mathlib.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

vk_potentially_visible_lights_t g_lights = {0};

vk_emissive_texture_table_t g_emissive_texture_table[MAX_TEXTURES];

typedef struct {
	const char *name;
	int r, g, b, intensity;
} vk_light_texture_rad_data;

static void loadRadData( const model_t *map, const char *filename ) {
	fs_offset_t size;
	const byte *data, *buffer = gEngine.COM_LoadFile( filename, &size, false);

	memset(g_emissive_texture_table, 0, sizeof(g_emissive_texture_table));

	if (!buffer) {
		gEngine.Con_Printf(S_ERROR "Couldn't load rad data from file %s, the map will be completely black\n", filename);
		return;
	}

	data = buffer;
	for (;;) {
		string name;
		float r, g, b, scale;

		int num = sscanf(data, "%s %f %f %f %f", name, &r, &g, &b, &scale);
		if (num == 2) {
			r = g = b;
		} else if (num == 5) {
			scale /= 255.f;
			r *= scale;
			g *= scale;
			b *= scale;
		} else if (num == 4) {
			// Ok, rgb only, no scaling
		} else {
			gEngine.Con_Printf( "skipping rad entry %s\n", num ? name : "" );
			num = 0;
		}

		if (num != 0) {
			gEngine.Con_Printf("rad entry: %s %f %f %f\n", name, r, g, b);

			{
				const char *wad_name = NULL;
				char *texture_name = Q_strchr(name, '/');
				string texname;

				if (!texture_name) {
					texture_name = name;
				} else {
					// name is now just a wad name
					texture_name[0] = '\0';
					wad_name = name;

					texture_name += 1;
				}

				// Try bsp texture first
				Q_sprintf(texname, "#%s:%s.mip", map->name, texture_name);
				int tex_id = VK_FindTexture(texname);
				gEngine.Con_Reportf("Looked up texture %s -> %d\n", texname, tex_id);

				// Try wad texture if bsp is not there
				if (!tex_id && wad_name) {
					Q_sprintf(texname, "%s.wad/%s.mip", wad_name, texture_name);
					tex_id = VK_FindTexture(texname);
					gEngine.Con_Reportf("Looked up texture %s -> %d\n", texname, tex_id);
				}

				if (tex_id) {
					ASSERT(tex_id < MAX_TEXTURES);

					g_emissive_texture_table[tex_id].emissive[0] = r;
					g_emissive_texture_table[tex_id].emissive[1] = g;
					g_emissive_texture_table[tex_id].emissive[2] = b;
					g_emissive_texture_table[tex_id].set = true;
				}
			}
		}

		data = Q_strchr(data, '\n');
		if (!data)
			break;
		while (!isalnum(*data)) ++data;
	}

	Mem_Free(buffer);
}

static void parseStaticLightEntities( void ) {
	const model_t* const world = gEngine.pfnGetModelByIndex( 1 );
	char *pos;
	enum {
		Unknown,
		Light, LightSpot,
	} classname = Unknown;
	struct {
		vec3_t origin;
		vec3_t color;
		//float radius;
		int style;
	} light = {0};
	enum {
		HaveOrigin = 1,
		HaveColor = 2,
		//HaveStyle = 4,
		HaveClass = 8,
		HaveAll = HaveOrigin | HaveColor | HaveClass,
	};
	unsigned int have = 0;

	ASSERT(world);

	pos = world->entities;
	for (;;) {
		string key, value;

		pos = gEngine.COM_ParseFile(pos, key);
		if (!pos)
			break;
		if (key[0] == '{') {
			classname = Unknown;
			have = 0;
			continue;
		}
		if (key[0] == '}') {
			// TODO handle entity
			if (have != HaveAll)
				continue;
			if (classname != Light && classname != LightSpot)
				continue;

			// TODO store this
			//VK_RenderAddStaticLight(light.origin, light.color);
			continue;
		}

		pos = gEngine.COM_ParseFile(pos, value);
		if (!pos)
			break;

		if (Q_strcmp(key, "origin") == 0) {
			const int components = sscanf(value, "%f %f %f",
				&light.origin[0],
				&light.origin[1],
				&light.origin[2]);
			if (components == 3)
				have |= HaveOrigin;
		} else
		if (Q_strcmp(key, "_light") == 0) {
			float scale = 1.f / 255.f;
			const int components = sscanf(value, "%f %f %f %f",
				&light.color[0],
				&light.color[1],
				&light.color[2],
				&scale);
			if (components == 1) {
				light.color[2] = light.color[1] = light.color[0] = light.color[0] / 255.f;
				have |= HaveColor;
			} else if (components == 4) {
				scale /= 255.f * 255.f;
				light.color[0] *= scale;
				light.color[1] *= scale;
				light.color[2] *= scale;
				have |= HaveColor;
			} else if (components == 3) {
				light.color[0] *= scale;
				light.color[1] *= scale;
				light.color[2] *= scale;
				have |= HaveColor;
			}
		} else if (Q_strcmp(key, "classname") == 0) {
			if (Q_strcmp(value, "light") == 0)
				classname = Light;
			else if (Q_strcmp(value, "light_spot") == 0)
				classname = LightSpot;
			have |= HaveClass;
		}
	}
}

// FIXME copied from mod_bmodel.c
// TODO would it be possible to not decompress each time, but instead get a list of all leaves?
static byte		g_visdata[(MAX_MAP_LEAFS+7)/8];	// intermediate buffer
byte *Mod_DecompressPVS( const byte *in, int visbytes )
{
	byte	*out;
	int	c;

	out = g_visdata;

	if( !in )
	{
		// no vis info, so make all visible
		while( visbytes )
		{
			*out++ = 0xff;
			visbytes--;
		}
		return g_visdata;
	}

	do
	{
		if( *in )
		{
			*out++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;

		while( c )
		{
			*out++ = 0;
			c--;
		}
	} while( out - g_visdata < visbytes );

	return g_visdata;
}

#define PR(...) gEngine.Con_Reportf(__VA_ARGS__)

static void DumpLeaves( void ) {
	model_t	*map = gEngine.pfnGetModelByIndex( 1 );
	const world_static_t *world = gEngine.GetWorld();
	ASSERT(map);

	PR("visbytes=%d leafs: %d:\n", world->visbytes, map->numleafs);
	for (int i = 0; i < map->numleafs; ++i) {
		const mleaf_t* leaf = map->leafs + i;
		PR("  %d: contents=%d numsurfaces=%d cluster=%d\n",
			i, leaf->contents, leaf->nummarksurfaces, leaf->cluster);

		// TODO: mark which surfaces belong to which leaves
		// TODO: figure out whether this relationship is stable (surface belongs to only one leaf)

		// print out PVS
		{
			int pvs_count = 0;
			const byte *visdata = Mod_DecompressPVS(leaf->compressed_vis, world->visbytes);
			if (!visdata) continue;
			PR("    PVS:");
			for (int j = 0; j < map->numleafs; ++j) {
				if (CHECKVISBIT(visdata, map->leafs[j].cluster /* FIXME cluster (j+1) or j??!?!*/)) {
					pvs_count++;
					PR(" %d", j);
				}
			}
			PR(" TOTAL: %d\n", pvs_count);
		}
	}
}

typedef struct {
	model_t	*map;
	const world_static_t *world;
	FILE *f;
} traversal_context_t;

static void visitLeaf(const mleaf_t *leaf, const mnode_t *parent, const traversal_context_t *ctx) {
	const int parent_index = parent - ctx->map->nodes;
	int pvs_count = 0;
	const byte *visdata = Mod_DecompressPVS(leaf->compressed_vis, ctx->world->visbytes);
	int num_emissive = 0;

	// ??? empty leaf?
	if (leaf->cluster < 0) // || leaf->nummarksurfaces == 0)
		return;

	fprintf(ctx->f, "\"N%d\" -> \"L%d\"\n", parent_index, leaf->cluster);
	for (int i = 0; i < leaf->nummarksurfaces; ++i) {
		const msurface_t *surf = leaf->firstmarksurface[i];
		const int surf_index = surf - ctx->map->surfaces;
		const int texture_num = surf->texinfo->texture->gl_texturenum;
		const qboolean emissive = texture_num >= 0 && g_emissive_texture_table[texture_num].set;

		if (emissive) num_emissive++;

		fprintf(ctx->f, "L%d -> S%d [color=\"#%s\"; dir=\"none\"];\n",
			leaf->cluster, surf_index, emissive ? "ff0000ff" : "00000040");
	}

	if (!visdata)
		return;

	for (int j = 0; j < ctx->map->numleafs; ++j) {
		if (CHECKVISBIT(visdata, ctx->map->leafs[j].cluster)) {
			pvs_count++;
		}
	}

	fprintf(ctx->f, "\"L%d\" [label=\"Leaf cluster %d\\npvs_count: %d\\nummarksurfaces: %d\\n num_emissive: %d\"; style=filled; fillcolor=\"%s\"; ];\n",
		leaf->cluster, leaf->cluster, pvs_count, leaf->nummarksurfaces, num_emissive,
		num_emissive > 0 ? "red" : "transparent"
		);
}

static void visitNode(const mnode_t *node, const mnode_t *parent, const traversal_context_t *ctx) {
	if (node->contents < 0) {
		visitLeaf((const mleaf_t*)node, parent, ctx);
	} else {
		const int parent_index = parent ? parent - ctx->map->nodes : -1;
		const int node_index = node - ctx->map->nodes;
		fprintf(ctx->f, "\"N%d\" -> \"N%d\"\n", parent_index, node_index);
		fprintf(ctx->f, "\"N%d\" [label=\"numsurfaces: %d\\nfirstsurface: %d\"];\n",
			node_index, node->numsurfaces, node->firstsurface);
		visitNode(node->children[0], node, ctx);
		visitNode(node->children[1], node, ctx);
	}
}

static void traverseBSP( void ) {
	const traversal_context_t ctx = {
		.map = gEngine.pfnGetModelByIndex( 1 ),
		.world = gEngine.GetWorld(),
		.f = fopen("bsp.dot", "w"),
	};

	fprintf(ctx.f, "digraph bsp { node [shape=box];\n");
	visitNode(ctx.map->nodes, NULL, &ctx);
	fprintf(ctx.f,
		"subgraph surfaces {rank = max; style= filled; color = lightgray;\n");
	for (int i = 0; i < ctx.map->numsurfaces; i++) {
		const msurface_t *surf = ctx.map->surfaces + i;
		const int texture_num = surf->texinfo->texture->gl_texturenum;
		fprintf(ctx.f, "S%d [rank=\"max\"; label=\"S%d\\ntexture: %s\\nnumedges: %d\\ntexture_num=%d\"; style=filled; fillcolor=\"%s\";];\n",
			i, i,
			surf->texinfo && surf->texinfo->texture ? surf->texinfo->texture->name : "NULL",
			surf->numedges, texture_num,
			(texture_num >= 0 && g_emissive_texture_table[texture_num].set) ? "red" : "transparent" );
	}
	fprintf(ctx.f, "}\n}\n");
	fclose(ctx.f);
	//exit(0);
}

static void buildStaticMapEmissiveSurfaces( void ) {
	const model_t *map = gEngine.pfnGetModelByIndex( 1 );

	// Initialize emissive surface table
	for (int i = map->firstmodelsurface; i < map->firstmodelsurface + map->nummodelsurfaces; ++i) {
		const msurface_t *surface = map->surfaces + i;
		const int texture_num = surface->texinfo->texture->gl_texturenum;

		// TODO animated textures ???
		if (!g_emissive_texture_table[texture_num].set)
			continue;

		if (g_lights.num_emissive_surfaces < 256) {
			vk_emissive_surface_t *esurf = g_lights.emissive_surfaces + g_lights.num_emissive_surfaces;

			esurf->surface_index = i;
			VectorCopy(g_emissive_texture_table[texture_num].emissive, esurf->emissive);
		}

		++g_lights.num_emissive_surfaces;
	}

	gEngine.Con_Reportf("Emissive surfaces found: %d\n", g_lights.num_emissive_surfaces);

	if (g_lights.num_emissive_surfaces > UINT8_MAX + 1) {
		gEngine.Con_Printf(S_ERROR "Too many emissive surfaces found: %d; some areas will be dark\n", g_lights.num_emissive_surfaces);
		g_lights.num_emissive_surfaces = UINT8_MAX + 1;
	}
}

static void addSurfaceLightToCell( const int light_cell[3], int emissive_surface_index ) {
	const uint cluster_index = light_cell[0] + light_cell[1] * g_lights.grid.size[0] + light_cell[2] * g_lights.grid.size[0] * g_lights.grid.size[1];
	vk_light_cluster_t *cluster = g_lights.grid.cells + cluster_index;

	if (light_cell[0] < 0 || light_cell[1] < 0 || light_cell[2] < 0
		|| (light_cell[0] >= g_lights.grid.size[0])
		|| (light_cell[1] >= g_lights.grid.size[1])
		|| (light_cell[2] >= g_lights.grid.size[2]))
		return;

	if (cluster->num_emissive_surfaces == MAX_VISIBLE_SURFACE_LIGHTS) {
		gEngine.Con_Printf(S_ERROR "Cluster %d,%d,%d(%d) ran out of emissive surfaces slots\n",
			light_cell[0], light_cell[1],  light_cell[2], cluster_index
			);
		return;
	}

	cluster->emissive_surfaces[cluster->num_emissive_surfaces] = emissive_surface_index;
	++cluster->num_emissive_surfaces;
}

static void buildStaticMapLightsGrid( void ) {
	const model_t	*map = gEngine.pfnGetModelByIndex( 1 );

	// 1. Determine map bounding box (and optimal grid size?)
		// map->mins, maxs
	vec3_t map_size, min_cell, max_cell;
	VectorSubtract(map->maxs, map->mins, map_size);

	VectorDivide(map->mins, LIGHT_GRID_CELL_SIZE, min_cell);
	min_cell[0] = floorf(min_cell[0]);
	min_cell[1] = floorf(min_cell[1]);
	min_cell[2] = floorf(min_cell[2]);
	VectorCopy(min_cell, g_lights.grid.min_cell);

	VectorDivide(map->maxs, LIGHT_GRID_CELL_SIZE, max_cell);
	max_cell[0] = ceilf(max_cell[0]);
	max_cell[1] = ceilf(max_cell[1]);
	max_cell[2] = ceilf(max_cell[2]);

	VectorSubtract(max_cell, min_cell, g_lights.grid.size);
	g_lights.grid.num_cells = g_lights.grid.size[0] * g_lights.grid.size[1] * g_lights.grid.size[2];
	ASSERT(g_lights.grid.num_cells < MAX_LIGHT_CLUSTERS);

	gEngine.Con_Reportf("Map mins:(%f, %f, %f), maxs:(%f, %f, %f), size:(%f, %f, %f), min_cell:(%f, %f, %f) cells:(%d, %d, %d); total: %d\n",
		map->mins[0], map->mins[1], map->mins[2],
		map->maxs[0], map->maxs[1], map->maxs[2],
		map_size[0], map_size[1], map_size[2],
		min_cell[0], min_cell[1], min_cell[2],
		g_lights.grid.size[0],
		g_lights.grid.size[1],
		g_lights.grid.size[2],
		g_lights.grid.num_cells
	);

	// 3. For all light sources
	for (int i = 0; i < g_lights.num_emissive_surfaces; ++i) {
		const vk_emissive_surface_t *emissive = g_lights.emissive_surfaces + i;
		const msurface_t *surface = map->surfaces + emissive->surface_index;
		int cluster_index;
		vk_light_cluster_t *cluster;
		vec3_t light_cell;
		float effective_radius;
		const float intensity_threshold = 1.f / 255.f; // TODO better estimate
		const float intensity = Q_max(Q_max(emissive->emissive[0], emissive->emissive[1]), emissive->emissive[2]);
		ASSERT(surface->info);

		// FIXME using just origin is incorrect
		{
			vec3_t light_cell_f;
			VectorDivide(surface->info->origin, LIGHT_GRID_CELL_SIZE, light_cell_f);
			light_cell[0] = floorf(light_cell_f[0]);
			light_cell[1] = floorf(light_cell_f[1]);
			light_cell[2] = floorf(light_cell_f[2]);
		}
		VectorSubtract(light_cell, g_lights.grid.min_cell, light_cell);

		ASSERT(light_cell[0] >= 0);
		ASSERT(light_cell[1] >= 0);
		ASSERT(light_cell[2] >= 0);
		ASSERT(light_cell[0] < g_lights.grid.size[0]);
		ASSERT(light_cell[1] < g_lights.grid.size[1]);
		ASSERT(light_cell[2] < g_lights.grid.size[2]);

		//		3.3	Add it to those cells
		effective_radius = sqrtf(intensity / intensity_threshold);
		{
			const int irad = ceilf(effective_radius / LIGHT_GRID_CELL_SIZE);
			gEngine.Con_Reportf("Emissive surface %d: max intensity: %f; eff rad: %f; cell rad: %d\n", i, intensity, effective_radius, irad);
			for (int x = -irad; x <= irad; ++x)
				for (int y = -irad; y <= irad; ++y)
					for (int z = -irad; z <= irad; ++z) {
						const int cell[3] = { light_cell[0] + x, light_cell[1] + y, light_cell[2] + z};
						// TODO culling, ...
						// 		3.1 Compute light size and intensity (?)
						//		3.2 Compute which cells it might affect
						//			- light orientation
						//			- light intensity
						//			- PVS
						addSurfaceLightToCell(cell, i);
					}
		}
	}

	// Print light grid stats
	{
		#define GROUPSIZE 4
		int histogram[1 + (MAX_VISIBLE_SURFACE_LIGHTS + GROUPSIZE - 1) / GROUPSIZE] = {0};
		for (int i = 0; i < g_lights.grid.num_cells; ++i) {
			const vk_light_cluster_t *cluster = g_lights.grid.cells + i;
			const int hist_index = cluster->num_emissive_surfaces ? 1 + cluster->num_emissive_surfaces / GROUPSIZE : 0;
			histogram[hist_index]++;
		}

		gEngine.Con_Reportf("Built %d light clusters. Stats:\n", g_lights.grid.num_cells);
		gEngine.Con_Reportf("  0: %d\n", histogram[0]);
		for (int i = 1; i < ARRAYSIZE(histogram); ++i)
			gEngine.Con_Reportf("  %d-%d: %d\n",
				(i - 1) * GROUPSIZE,
				i * GROUPSIZE - 1,
				histogram[i]);
	}

	{
		for (int i = 0; i < g_lights.grid.num_cells; ++i) {
			const vk_light_cluster_t *cluster = g_lights.grid.cells + i;
			if (cluster->num_emissive_surfaces > 0) {
				gEngine.Con_Reportf(" cluster %d: emissive_surfaces=%d\n", i, cluster->num_emissive_surfaces);
			}
		}
	}
}

void VK_LightsLoadMap( void ) {
	const model_t *map = gEngine.pfnGetModelByIndex( 1 );

	parseStaticLightEntities();

	g_lights.num_emissive_surfaces = 0;
	g_lights.grid.num_cells = 0;
	memset(&g_lights.grid, 0, sizeof(g_lights.grid));

	// FIXME ...
	//initHackRadTable();

	// Load RAD data based on map name
	loadRadData( map, "rad/lights_anomalous_materials.rad" );

	buildStaticMapEmissiveSurfaces();
	buildStaticMapLightsGrid();
}

void VK_LightsShutdown( void ) {
}
