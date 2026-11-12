/*
 * Clean-room GPL implementation of SiN Game Module (API 14)
 * Edict allocation and management utilities.
 */
#include "g_local.h"

void G_InitEdicts (void)
{
	int i;

	if (!game.edicts)
	{
		game.max_edicts = MAX_EDICTS;
		game.edicts = (edict_t *)calloc (game.max_edicts, sizeof(edict_t));
	}
	else
	{
		memset (game.edicts, 0, game.max_edicts * sizeof(edict_t));
	}

	game.num_edicts = 0;

	/* Reserve slot 0 for worldspawn */
	game.edicts[0].s.number = 0;
	game.edicts[0].inuse = true;
	game.num_edicts = 1;

	/* Reserve slots 1..max_clients for player edicts */
	for (i = 1; i <= game.max_clients && i < game.max_edicts; i++)
	{
		game.edicts[i].s.number = i;
		game.edicts[i].inuse = false;
		if (game.clients)
			game.edicts[i].client = &game.clients[i - 1];
	}
	if (game.max_clients >= game.num_edicts)
		game.num_edicts = game.max_clients + 1;
}

edict_t *G_AllocEdict (void)
{
	int i;

	/* Allocate non-player edicts from max_clients + 1 upwards */
	for (i = game.max_clients + 1; i < game.max_edicts; i++)
	{
		if (!game.edicts[i].inuse)
		{
			edict_t *e = &game.edicts[i];
			memset (e, 0, sizeof(*e));
			e->s.number = i;
			e->inuse = true;
			if (i >= game.num_edicts)
				game.num_edicts = i + 1;
			return e;
		}
	}

	gi.dprintf ("G_AllocEdict: out of edicts\n");
	return NULL;
}

void G_FreeEdict (edict_t *ed)
{
	if (!ed || !ed->inuse)
		return;

	gi.unlinkentity (ed);
	memset (ed, 0, sizeof(*ed));
	ed->inuse = false;
}
