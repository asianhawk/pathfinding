#include <lua.h>
#include <lauxlib.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define SEARCH_DEPTH 1024

/*
   0  1  2 
    \ | /
  7 -   - 3
    / | \
   6  5  4
 */

struct map {
	int width;
	int height;
	uint8_t m[0];
};

struct pathnode {
	int x;
	int y;
	int camefrom;
	int gscore;
	int fscore;
};

struct path {
	int depth;
	int *set;
	struct pathnode *n;
};

static void
addbuilding(lua_State *L, struct map *m, int x, int y, int size) {
	if (x < 0 || x + size > m->width ||
		y < 0 || y + size > m->height) {
		luaL_error(L, "building (%d,%d,%d) is out of map", x,y,size);
	}
#define M(bit) (1<<bit)
#define SET(xx,yy,bits) { \
	uint8_t * b = &m->m[(y+yy)*(m->width+1) + (x+xx)];\
	uint8_t mask = bits; \
	if ((*b & mask) != 0) { \
		luaL_error(L, "Can't add building (%d,%d,%d)", x,y,size); \
	} else {	\
		*b |= mask;	\
	}	\
}
	int i,j;
	SET(0,   0, M(4))	// up-left
	SET(size,0, M(6))	// up-right
	SET(0,size, M(2))	// bottom-left
	SET(size,size, M(0))	// bottom-right
	for (i=1;i<size;i++) {
		SET(i,0, M(6) | M(5) | M(4))		// up
		SET(i, size, M(0) | M(1) | M(2))	// bottom
		SET(0, i, M(2) | M(3) | M(4))		// left
		SET(size, i, M(0) | M(7) | M(6))	// right
		for (j=1;j<size;j++) {
			SET(i,j, 0xff)	// center
		}
	}
#undef M
#undef SET
}

static int
getfield(lua_State *L, int index, const char *f) {
	if (lua_getfield(L, -1, f) != LUA_TNUMBER) {
		return luaL_error(L, "invalid [%d].%s", index, f);
	}
	int v = lua_tointeger(L, -1);
	lua_pop(L, 1);
	return v;
}

static int
lnewmap(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	lua_settop(L, 1);
	int width = getfield(L, 0, "width");
	int height = getfield(L, 0, "height");
	lua_len(L, 1);
	int n = luaL_checkinteger(L, -1);
	lua_pop(L, 1);
	struct map *m = lua_newuserdata(L, sizeof(struct map) + (width+1) * (height+1) * sizeof(m->m[0]));
	m->width = width;
	m->height = height;
	memset(m->m, 0, (width+1) * (height+1) * sizeof(m->m[0]));
	int i;
#define M(bit) (1<<bit)
#define SET(xx,yy,bits) { m->m[yy * (width+1) + xx] = bits; }
	SET(0,0, M(2) | M(1) | M(0) | M(7) | M(6))	// up-left
	SET(width,0, M(0) | M(1) | M(2) | M(3) | M(4))	// up-right
	SET(0, height, M(0) | M(7) | M(6) | M(5) | M(4))	//  bottom-left
	SET(width,height, M(2) | M(3) | M(4) | M(5) | M(6))	// bottom-right
	for (i=1;i<width;i++) {
		SET(i,0, M(0) | M(1) | M(2))	// up
		SET(i,height, M(4) | M(5) | M(6))	// bottom
	}
	for (i=1;i<height;i++) {
		SET(0,i, M(0) | M(7) | M(6))	// left
		SET(width,i, M(2) | M(3) | M(4))	// bottom
	}
#undef M
#undef SET
	for (i=1;i<=n;i++) {
		if (lua_geti(L, 1, i) != LUA_TTABLE) {
			return luaL_error(L, "Invalid table index = %d", i);
		}
		int x = getfield(L, i, "x");
		int y = getfield(L, i, "y");
		int size = getfield(L, i, "size");
		lua_pop(L, 1);
		addbuilding(L, m, x, y, size);
	}

	return 1;
}

static int
lblock(lua_State *L) {
	luaL_checktype(L, 1, LUA_TUSERDATA);
	struct map *m = lua_touserdata(L, 1);
	int x = luaL_checkinteger(L, 2);
	int y = luaL_checkinteger(L, 3);
	if (x < 0 || x > m->width ||
		y < 0 || y > m->height) {
		luaL_error(L, "Position (%d,%d) is out of map", x,y);
	}
	uint8_t bits = m->m[y * (m->width+1) + x];
	int i;
	for (i=0;i<8;i++) {
		lua_pushboolean(L, bits & (1<<i));
	}

	return 8;
}

static inline int
distance(int x1, int y1, int x2, int y2) {
	int dx = x1 - x2;
	int dy = y1 - y2;
	if (dx < 0) {
		dx = -dx;
	}
	if (dy < 0) {
		dy = -dy;
	}
	if (dx < dy)
		return dx *  7 + (dy-dx) * 5;
	else
		return dy * 7 + (dx - dy) * 5;
}

struct context {
	struct path *P;
	int open;
	int closed;
	int n;
	int end_x;
	int end_y;
};

static struct pathnode *
add_open(struct context *ctx, int x, int y, int camefrom, int gscore) {
	struct path *P = ctx->P;
	if (ctx->n >= P->depth) {
		return NULL;
	}
	P->set[ctx->open++] = ctx->n;
	struct pathnode *pn = &P->n[ctx->n++];
	pn->x = x;
	pn->y = y;
	pn->camefrom = camefrom;
	pn->gscore = gscore;
	pn->fscore = gscore + distance(x,y,ctx->end_x,ctx->end_y);

	return pn;
};

static struct pathnode *
lowest_fscore(struct context *ctx) {
	int idx = 0;
	struct pathnode *pn = &ctx->P->n[ctx->P->set[idx]];
	int fscore = pn->fscore;
	int i;
	for (i=1;i<ctx->open;i++) {
		struct pathnode *tmp = &ctx->P->n[ctx->P->set[i]];
		if (tmp->fscore < fscore) {
			pn = tmp;
			idx = i;
			fscore = tmp->fscore;
		}
	}
	// remove from open set
	--ctx->open;
	if (idx != ctx->open) {
		ctx->P->set[idx] = ctx->P->set[ctx->open];
	}
	return pn;
}

static void
add_closed(struct context *ctx, int idx) {
	ctx->P->set[ctx->P->depth - 1 - ctx->closed++] = idx;
}

static int
in_closed(struct context *ctx, int x, int y) {
	int i;
	for (i=0;i<ctx->closed;i++) {
		int idx = ctx->P->set[ctx->P->depth - 1 - i];
		struct pathnode * pn = &ctx->P->n[idx];
		if (pn->x == x && pn->y == y)
			return 1;
	}
	return 0;
}

static struct pathnode *
find_open(struct context *ctx, int x, int y) {
	int i;
	for (i=0;i<ctx->open;i++) {
		int idx = ctx->P->set[i];
		struct pathnode * pn = &ctx->P->n[idx];
		if (pn->x == x && pn->y == y)
			return pn;
	}
	return NULL;
}

static int
nearest(struct path *P, int from, int to) {
	int ret = P->set[from];
	struct pathnode *pn = &P->n[ret];
	int fscore = pn->fscore;
	int i;
	for (i=from+1;i<to;i++) {
		int idx = P->set[i];
		pn = &P->n[idx];
		if (pn->fscore < fscore) {
			fscore = pn->fscore;
			ret = idx;
		}
	}
	return ret;
}

static int
path_finding(struct map *m, struct path *P, int start_x, int start_y, int end_x, int end_y) {
	struct context ctx;
	ctx.P = P;
	ctx.open = 0;
	ctx.closed = 0;
	ctx.n = 0;
	ctx.end_x = end_x;
	ctx.end_y = end_y;
	add_open(&ctx, start_x, start_y, -1, 0);
	while(ctx.open > 0) {
		struct pathnode * pn = lowest_fscore(&ctx);
		int current = pn - P->n;
		if (pn->x == end_x && pn->y == end_y)
			return current;
		add_closed(&ctx, current);
		uint8_t bits = m->m[pn->y * (m->width+1) + pn->x];
		int i;
		static struct {
			int dx;
			int dy;
			int distance;
		} off[8] = {
			{ -1, -1, 7 },	// up-left
			{  0, -1, 5 },	// up
			{  1, -1, 7 },	// up-right
			{  1,  0, 5 },	// right
			{  1,  1, 7 },	// bottom-right
			{  0,  1, 5 },	// bottom
			{  -1, 1, 7 },	// bottom-left
			{  -1, 0, 5 },	// left
		};
		for (i=0;i<8;i++) {
			if (bits & (1<<i))
				continue;
			int x = pn->x + off[i].dx;
			int y = pn->y + off[i].dy;
			if (in_closed(&ctx, x , y))
				continue;
			int tentative_gscore = pn->gscore + off[i].distance;
			struct pathnode * neighbor = find_open(&ctx, x, y);
			if (neighbor) {
				if (tentative_gscore < neighbor->gscore) {
					neighbor->camefrom = current;
					neighbor->gscore = tentative_gscore;
					neighbor->fscore = tentative_gscore + distance(x,y,end_x,end_y);
				}
			} else if (add_open(&ctx, x, y, current, tentative_gscore) == NULL) {
				break;
			}

		}
	}
	if (ctx.open > 0) {
		return nearest(P, 0, ctx.open);
	} else {
		return nearest(P, P->depth - ctx.closed, P->depth);
	}
}

static void
close_path(struct path *P) {
	if (P->depth > SEARCH_DEPTH) {
		free(P->set);
		free(P->n);
	}
}

static void
check_position(lua_State *L, struct map *m, int x, int y) {
	if (x < 0 || x > m->width ||
		y < 0 || y > m->height) {
		luaL_error(L, "Invalid position (%d,%d)", x,y);
	}
}

static int
lpath(lua_State *L) {
	luaL_checktype(L, 1, LUA_TUSERDATA);
	struct map *m = lua_touserdata(L, 1);
	int start_x = luaL_checkinteger(L, 2);
	int start_y = luaL_checkinteger(L, 3);
	int end_x = luaL_checkinteger(L, 4);
	int end_y = luaL_checkinteger(L, 5);

	check_position(L, m, start_x, start_y);
	check_position(L, m, end_x, end_y);

	struct path P;
	P.depth = luaL_optinteger(L, 6, 1024);
	int stack_size = P.depth > SEARCH_DEPTH ? 0 : P.depth;
	int set[stack_size];
	struct pathnode pn[stack_size];
	if (P.depth > SEARCH_DEPTH) {
		P.set = malloc(sizeof(int) * P.depth);
		P.n = malloc(sizeof(struct pathnode) * P.depth);
	} else {
		P.set = set;
		P.n = pn;
	}

	int node = path_finding(m, &P, start_x, start_y, end_x, end_y);
	int n = 1;
	int idx = node;
	while (P.n[idx].camefrom >= 0) {
		idx = P.n[idx].camefrom;
		++n;
	}

	struct {
		int x;
		int y;
	} pos[n];

	int i;

	for (i=0;i<n;i++) {
		struct pathnode *pn = &P.n[node];
		pos[i].x = pn->x;
		pos[i].y = pn->y;
		node = P.n[node].camefrom;
	}

	close_path(&P);

	lua_settop(L, 0);
	luaL_checkstack(L, n * 2, NULL);
	for (i=n-1;i>=0;i--) {
		lua_pushinteger(L, pos[i].x);
		lua_pushinteger(L, pos[i].y);
	}

	return n * 2;
}

int
luaopen_pathfinding(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{ "new", lnewmap },
		{ "block", lblock },
		{ "path", lpath },
		{ NULL, NULL },
	};
	luaL_newlib(L,l);
	return 1;
}
