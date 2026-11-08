/*
===========================================================================

Return to Castle Wolfenstein multiplayer GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company. 

This file is part of the Return to Castle Wolfenstein multiplayer GPL Source Code (RTCW MP Source Code).  

RTCW MP Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RTCW MP Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RTCW MP Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the RTCW MP Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the RTCW MP Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

//===========================================================================
//
// Name:         map_q3.c
// Function:     map loading
// Programmer:   Mr Elusive (MrElusive@demigod.demon.nl)
// Last update:  1999-07-02
// Tab Size:     3
//===========================================================================

#include "qbsp.h"
#include "l_mem.h"
#include "..\botlib\aasfile.h"         //aas_bbox_t
#include "aas_store.h"       //AAS_MAX_BBOXES
#include "aas_cfg.h"
#include "aas_map.h"         //AAS_CreateMapBrushes
#include "l_bsp_iw3.h"
#include "..\game\surfaceflags.h"
#include "cm_patch.h"

#define NODESTACKSIZE       1024
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void PrintContents( int contents );

#define NODESTACKSIZE       1024

int nodestack[NODESTACKSIZE];
int *nodestackptr;
int nodestacksize = 0;
int brushmodelnumbers[MAX_MAPFILE_BRUSHES];
int dbrushleafnums[MAX_MAPFILE_BRUSHES];
int dplanes2mapplanes[MAX_MAPFILE_PLANES];

int IW3_BrushContents( mapbrush_t *b ) {
	int contents, i, mixed, hint;
	side_t *s;

	s = &b->original_sides[0];
	contents = s->contents;
	//
	mixed = false;
	hint = false;
	for ( i = 1; i < b->numsides; i++ )
	{
		s = &b->original_sides[i];
		if ( s->contents != contents ) {
			mixed = true;
		}
		if ( s->surf & ( SURF_HINT | SURF_SKIP ) ) {
			hint = true;
		}
		contents |= s->contents;
	} //end for
	  //
	if ( hint ) {
		if ( contents ) {
			Log_Write( "WARNING: hint brush with contents: " );
			PrintContents( contents );
			Log_Write( "\r\n" );
			//
			Log_Write( "brush contents is: " );
			PrintContents( b->contents );
			Log_Write( "\r\n" );
		} //end if
		return 0;
	} //end if
	  //Log_Write("brush %d contents ", nummapbrushes);
	  //PrintContents(contents);
	  //Log_Write("\r\n");
	  //remove ladder and fog contents
	contents &= ~( CONTENTS_LADDER | CONTENTS_FOG );
	//
	if ( mixed ) {
		Log_Write( "Entity %i, Brush %i: mixed face contents "
				   , b->entitynum, b->brushnum );
		PrintContents( contents );
		Log_Write( "\r\n" );
		//
		Log_Write( "brush contents is: " );
		PrintContents( b->contents );
		Log_Write( "\r\n" );
		//
		if ( contents & CONTENTS_DONOTENTER ) {
			return CONTENTS_DONOTENTER;                                //Log_Print("mixed contents with donotenter\n");
		}
		/*
		Log_Print("contents:"); PrintContents(contents);
		Log_Print("\ncontents:"); PrintContents(s->contents);
		Log_Print("\n");
		Log_Print("texture name = %s\n", texinfo[s->texinfo].texture);
		*/
		//if liquid brush
		if ( contents & ( CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_WATER ) ) {
			return ( contents & ( CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_WATER ) );
		} //end if
		if ( contents & CONTENTS_PLAYERCLIP ) {
			return ( contents & CONTENTS_PLAYERCLIP );
		}
		return ( contents & CONTENTS_SOLID );
	} //end if
	  /*
	  if (contents & CONTENTS_AREAPORTAL)
	  {
		  static int num;
		  Log_Write("Entity %i, Brush %i: area portal %d\r\n", b->entitynum, b->brushnum, num++);
	  } //end if*/
	if ( contents == ( contents & CONTENTS_STRUCTURAL ) ) {
		//Log_Print("brush %i is only structural\n", b->brushnum);
		contents = 0;
	} //end if
	if ( contents & CONTENTS_DONOTENTER ) {
		Log_Print( "brush %i is a donotenter brush, c = %X\n", b->brushnum, contents );
	} //end if
	return contents;
} //end of the function Q3_BrushContents
#define BBOX_NORMAL_EPSILON         0.0001
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void IW3_DPlanes2MapPlanes( void ) {
	int i;

	for ( i = 0; i < iw3_numplanes; i++ )
	{
		dplanes2mapplanes[i] = FindFloatPlane( iw3_dplanes[i].normal, iw3_dplanes[i].dist );
	} //end for
} //end of the function Q3_DPlanes2MapPlanes
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void IW3_BSPBrushToMapBrush( q3_dbrush_t *bspbrush, entity_t *mapent ) {
	mapbrush_t *b;
	int i, k, n;
	side_t *side, *s2;
	int planenum;
	q3_dbrushside_t *bspbrushside;
	q3_dplane_t *bspplane;
	int contentFlags = 0;

	if ( nummapbrushes >= MAX_MAPFILE_BRUSHES ) {
		Error( "nummapbrushes >= MAX_MAPFILE_BRUSHES" );
	}

	b = &mapbrushes[nummapbrushes];
	b->original_sides = &brushsides[nummapbrushsides];
	b->entitynum = mapent - entities;
	b->brushnum = nummapbrushes - mapent->firstbrush;
	b->leafnum = dbrushleafnums[bspbrush - iw3_dbrushes];

	for ( n = 0; n < bspbrush->numSides; n++ )
	{
		//pointer to the bsp brush side
		bspbrushside = &iw3_dbrushsides[bspbrush->firstSide + n];

		if ( nummapbrushsides >= MAX_MAPFILE_BRUSHSIDES ) {
			Error( "MAX_MAPFILE_BRUSHSIDES" );
		} //end if
		  //pointer to the map brush side
		side = &brushsides[nummapbrushsides];
		//if the BSP brush side is textured
		if ( iw3_dbrushsidetextured[bspbrush->firstSide + n] ) {
			side->flags |= SFL_TEXTURED | SFL_VISIBLE;
		} else { side->flags &= ~SFL_TEXTURED;}
		//NOTE: all Quake3 sides are assumed textured
		//side->flags |= SFL_TEXTURED|SFL_VISIBLE;
		//
		if ( bspbrushside->shaderNum < 0 ) {
			side->contents = 0;
			side->surf = 0;
		} //end if
		else
		{
			side->contents = iw3_dshaders[bspbrushside->shaderNum].contentFlags;
			side->surf = iw3_dshaders[bspbrushside->shaderNum].surfaceFlags;
			if ( strstr( iw3_dshaders[bspbrushside->shaderNum].shader, "common/hint" ) ) {
				//Log_Print("found hint side\n");
				side->surf |= SURF_HINT;
			} //end if

			// Ridah, mark ladder brushes
			if ( strstr( iw3_dshaders[bspbrushside->shaderNum].shader, "common/ladder" ) ) {
				//Log_Print("found ladder side\n");
				side->contents |= CONTENTS_LADDER;
				contentFlags |= CONTENTS_LADDER;
			} //end if
			  // done.

		} //end else
		  //

		if ( !( strstr( iw3_dshaders[bspbrushside->shaderNum].shader, "common/slip" ) ) ) {
			side->flags |= SFL_VISIBLE;
		} else if ( side->surf & SURF_NODRAW )     {
			side->flags |= SFL_TEXTURED | SFL_VISIBLE;
		} //end if
		  /*
		  if (side->contents & (CONTENTS_TRANSLUCENT|CONTENTS_STRUCTURAL))
		  {
			  side->flags |= SFL_TEXTURED|SFL_VISIBLE;
		  } //end if*/

		// hints and skips are never detail, and have no content
		if ( side->surf & ( SURF_HINT | SURF_SKIP ) ) {
			side->contents = 0;
			//Log_Print("found hint brush side\n");
		}
		/*
		if ((side->surf & SURF_NODRAW) && (side->surf & SURF_NOIMPACT))
		{
			side->contents = 0;
			side->surf &= ~CONTENTS_DETAIL;
			Log_Print("probably found hint brush in a BSP without hints being used\n");
		} //end if*/

		//ME: get a plane for this side
		bspplane = &iw3_dplanes[bspbrushside->planeNum];
		planenum = FindFloatPlane( bspplane->normal, bspplane->dist );
		//
		// see if the plane has been used already
		//
		//ME: this really shouldn't happen!!!
		//ME: otherwise the bsp file is corrupted??
		//ME: still it seems to happen, maybe Johny Boy's
		//ME: brush bevel adding is crappy ?
		for ( k = 0; k < b->numsides; k++ )
		{
			s2 = b->original_sides + k;
//			if (DotProduct (mapplanes[s2->planenum].normal, mapplanes[planenum].normal) > 0.999
//							&& fabs(mapplanes[s2->planenum].dist - mapplanes[planenum].dist) < 0.01 )

			if ( s2->planenum == planenum ) {
				Log_Print( "Entity %i, Brush %i: duplicate plane\n"
						   , b->entitynum, b->brushnum );
				break;
			}
			if ( s2->planenum == ( planenum ^ 1 ) ) {
				Log_Print( "Entity %i, Brush %i: mirrored plane\n"
						   , b->entitynum, b->brushnum );
				break;
			}
		}
		if ( k != b->numsides ) {
			continue;       // duplicated

		}
		//
		// keep this side
		//
		//ME: reset pointer to side, why? hell I dunno (pointer is set above already)
		side = b->original_sides + b->numsides;
		//ME: store the plane number
		side->planenum = planenum;
		//keep the material of this side, the CoD4 map writer needs it
		side->shadernum = bspbrushside->shaderNum;
		//ME: texinfo is already stored when bsp is loaded
		//NOTE: check for TEXINFO_NODE, otherwise crash in Q3_BrushContents
		//if (bspbrushside->texinfo < 0) side->texinfo = 0;
		//else side->texinfo = bspbrushside->texinfo;

		// save the td off in case there is an origin brush and we
		// have to recalculate the texinfo
		// ME: don't need to recalculate because it's already done
		//     (for non-world entities) in the BSP file
//		side_brushtextures[nummapbrushsides] = td;

		nummapbrushsides++;
		b->numsides++;
	} //end for

	// get the content for the entire brush
	//Quake3 bsp brushes don't have a contents
	b->contents = iw3_dshaders[bspbrush->shaderNum].contentFlags | contentFlags;
	// Ridah, Wolf has ladders (if we call Q3_BrushContents(), we'll get the solid area bug
	b->contents &= ~( /*CONTENTS_LADDER|*/ CONTENTS_FOG | CONTENTS_STRUCTURAL );
	//b->contents = Q3_BrushContents(b);
	//
	// Ridah, CONTENTS_MOSTERCLIP should prevent AAS from being created, but not clip players/AI in the game
	if ( b->contents & CONTENTS_MONSTERCLIP ) {
		b->contents |= CONTENTS_PLAYERCLIP;
	}

	// func_explosive's not solid (only matters for AAS, a map export keeps them)
	if ( create_aas && (
		 !strcmp( "func_explosive", ValueForKey( &entities[b->entitynum], "classname" ) ) ||
		 !strcmp( "func_invisible_user", ValueForKey( &entities[b->entitynum], "classname" ) ) ||
		 !strcmp( "func_static", ValueForKey( &entities[b->entitynum], "classname" ) ) ) ) {
		Log_Print( "Ignoring %s brush..\n", ValueForKey( &entities[b->entitynum], "classname" ) );
		b->numsides = 0;
		b->contents = 0;
		return;
	}

	if ( BrushExists( b ) ) {
		c_squattbrushes++;
		b->numsides = 0;
		return;
	} //end if

	//if we're creating AAS
	if ( create_aas ) {
		//create the AAS brushes from this brush, don't add brush bevels
		AAS_CreateMapBrushes( b, mapent, false );
		return;
	} //end if

	// allow detail brushes to be removed
	if ( nodetail && ( b->contents & CONTENTS_DETAIL ) ) {
		b->numsides = 0;
		return;
	} //end if

	// allow water brushes to be removed
	if ( nowater && ( b->contents & ( CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_WATER ) ) ) {
		b->numsides = 0;
		return;
	} //end if


	// create windings for sides and bounds for brush
	MakeBrushWindings( b );

	//mark brushes without winding or with a tiny window as bevels
	MarkBrushBevels( b );

	// brushes that will not be visible at all will never be
	// used as bsp splitters
	if ( b->contents & ( CONTENTS_PLAYERCLIP | CONTENTS_MONSTERCLIP ) ) {
		c_clipbrushes++;
		for ( i = 0; i < b->numsides; i++ )
			b->original_sides[i].texinfo = TEXINFO_NODE;
	} //end for

	//
	// origin brushes are removed, but they set
	// the rotation origin for the rest of the brushes
	// in the entity.  After the entire entity is parsed,
	// the planenums and texinfos will be adjusted for
	// the origin brush
	//
	//ME: not needed because the entities in the BSP file already
	//    have an origin set
//	if (b->contents & CONTENTS_ORIGIN)
//	{
//		char	string[32];
//		vec3_t	origin;
//
//		if (num_entities == 1)
//		{
//			Error ("Entity %i, Brush %i: origin brushes not allowed in world"
//				, b->entitynum, b->brushnum);
//			return;
//		}
//
//		VectorAdd (b->mins, b->maxs, origin);
//		VectorScale (origin, 0.5, origin);
//
//		sprintf (string, "%i %i %i", (int)origin[0], (int)origin[1], (int)origin[2]);
//		SetKeyValue (&entities[b->entitynum], "origin", string);
//
//		VectorCopy (origin, entities[b->entitynum].origin);
//
//		// don't keep this brush
//		b->numsides = 0;
//
//		return;
//	}

	//ME: the bsp brushes already have bevels, so we won't try to
	//    add them again (especially since Johny Boy's bevel adding might
	//    be crappy)
//	AddBrushBevels(b);

	nummapbrushes++;
	mapent->numbrushes++;
} //end of the function Q3_BSPBrushToMapBrush
//===========================================================================
//===========================================================================
void IW3_ParseBSPBrushes( entity_t *mapent ) {
	int i;

	/*
	//give all the brushes that belong to this entity the number of the
	//BSP model used by this entity
	Q3_SetBrushModelNumbers(mapent);
	//now parse all the brushes with the correct mapent->modelnum
	for (i = 0; i < iw3_numbrushes; i++)
	{
		if (brushmodelnumbers[i] == mapent->modelnum)
		{
			Q3_BSPBrushToMapBrush(&iw3_dbrushes[i], mapent);
		} //end if
	} //end for
	*/
	for ( i = 0; i < iw3_dmodels[mapent->modelnum].numBrushes; i++ )
	{
		IW3_BSPBrushToMapBrush( &iw3_dbrushes[iw3_dmodels[mapent->modelnum].firstBrush + i], mapent );
	} //end for
} //end of the function Q3_ParseBSPBrushes
//===========================================================================
//===========================================================================
qboolean IW3_ParseBSPEntity( int entnum ) {
	entity_t *mapent;
	char *model;
	int startbrush, startsides;

	startbrush = nummapbrushes;
	startsides = nummapbrushsides;

	mapent = &entities[entnum]; //num_entities];
	mapent->firstbrush = nummapbrushes;
	mapent->numbrushes = 0;
	mapent->modelnum = -1;  //-1 = no BSP model

	model = ValueForKey( mapent, "model" );
	if ( model && strlen( model ) ) {
		if ( *model == '*' ) {
			//get the model number of this entity (skip the leading *)
			mapent->modelnum = atoi( &model[1] );
		} //end if
	} //end if

	GetVectorForKey( mapent, "origin", mapent->origin );

	//if this is the world entity it has model number zero
	//the world entity has no model key
	if ( !strcmp( "worldspawn", ValueForKey( mapent, "classname" ) ) ) {
		mapent->modelnum = 0;
	} //end if
	  //if the map entity has a BSP model (a modelnum of -1 is used for
	  //entities that aren't using a BSP model)
	if ( mapent->modelnum >= 0 ) {
		//parse the bsp brushes
		IW3_ParseBSPBrushes( mapent );
	} //end if
	  //
	  //the origin of the entity is already taken into account
	  //
	  //func_group entities can't be in the bsp file
	  //
	  //check out the func_areaportal entities
	if ( !strcmp( "func_areaportal", ValueForKey( mapent, "classname" ) ) ) {
		c_areaportals++;
		mapent->areaportalnum = c_areaportals;
		return true;
	} //end if
	return true;
} //end of the function Q3_ParseBSPEntity
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
#define MAX_PATCH_VERTS     1024

static void AAS_CreateCurveBrushes( void ) {
	int i, j, n, planenum, numcurvebrushes = 0;
	q3_dsurface_t *surface;
	iw3_drawVert_t *dv_p;
	vec3_t points[MAX_PATCH_VERTS];
	int width, height, c;
	patchCollide_t *pc;
	facet_t *facet;
	mapbrush_t *brush;
	side_t *side;
	entity_t *mapent;
	winding_t *winding;

	qprintf( "nummapbrushsides = %d\n", nummapbrushsides );
	mapent = &entities[0];
	for ( i = 0; i < iw3_numDrawSurfaces; i++ )
	{
		surface = &iw3_drawSurfaces[i];
		//CoD4 surfaces are all triangle soups
		if ( surface->surfaceType != MST_PATCH || surface->patchWidth <= 0 || surface->patchHeight <= 0 ) {
			continue;
		}
		if ( surface->shaderNum < 0 || surface->shaderNum >= iw3_numShaders ) {
			continue;
		}
		//if the curve is not solid
		if ( !( iw3_dshaders[surface->shaderNum].contentFlags & ( CONTENTS_SOLID | CONTENTS_PLAYERCLIP ) ) ) {
			//Log_Print("skipped non-solid curve\n");
			continue;
		} //end if
		  //
		width = surface->patchWidth;
		height = surface->patchHeight;
		c = width * height;
		if ( c > MAX_PATCH_VERTS ) {
			Error( "ParseMesh: MAX_PATCH_VERTS %d", c );
		} //end if

		dv_p = iw3_drawVerts + surface->firstVert;
		for ( j = 0 ; j < c ; j++, dv_p++ )
		{
			points[j][0] = dv_p->xyz[0];
			points[j][1] = dv_p->xyz[1];
			points[j][2] = dv_p->xyz[2];
		} //end for
		  // create the internal facet structure
		pc = CM_GeneratePatchCollide( width, height, points );
		//
		for ( j = 0; j < pc->numFacets; j++ )
		{
			facet = &pc->facets[j];
			//
			brush = &mapbrushes[nummapbrushes];
			brush->original_sides = &brushsides[nummapbrushsides];
			brush->entitynum = 0;
			brush->brushnum = nummapbrushes - mapent->firstbrush;
			//
			brush->numsides = facet->numBorders + 2;
			nummapbrushsides += brush->numsides;
			brush->contents = CONTENTS_SOLID;
			//
			//qprintf("\r%6d curve brushes", nummapbrushsides);//++numcurvebrushes);
			qprintf( "\r%6d curve brushes", ++numcurvebrushes );
			//
			planenum = FindFloatPlane( pc->planes[facet->surfacePlane].plane, pc->planes[facet->surfacePlane].plane[3] );
			//
			side = &brush->original_sides[0];
			side->planenum = planenum;
			side->contents = CONTENTS_SOLID;
			side->flags |= SFL_TEXTURED | SFL_VISIBLE | SFL_CURVE;
			side->surf = 0;
			//
			side = &brush->original_sides[1];
			if ( create_aas ) {
				//the plane is expanded later so it's not a problem that
				//these first two opposite sides are coplanar
				side->planenum = planenum ^ 1;
			} //end if
			else
			{
				side->planenum = FindFloatPlane( mapplanes[planenum ^ 1].normal, mapplanes[planenum ^ 1].dist + 1 );
				side->flags |= SFL_TEXTURED | SFL_VISIBLE;
			} //end else
			side->contents = CONTENTS_SOLID;
			side->flags |= SFL_CURVE;
			side->surf = 0;
			//
			winding = BaseWindingForPlane( mapplanes[side->planenum].normal, mapplanes[side->planenum].dist );
			for ( n = 0; n < facet->numBorders; n++ )
			{
				//never use the surface plane as a border
				if ( facet->borderPlanes[n] == facet->surfacePlane ) {
					continue;
				}
				//
				side = &brush->original_sides[2 + n];
				side->planenum = FindFloatPlane( pc->planes[facet->borderPlanes[n]].plane, pc->planes[facet->borderPlanes[n]].plane[3] );
				if ( facet->borderInward[n] ) {
					side->planenum ^= 1;
				}
				side->contents = CONTENTS_SOLID;
				side->flags |= SFL_TEXTURED | SFL_CURVE;
				side->surf = 0;
				//chop the winding in place
				if ( winding ) {
					ChopWindingInPlace( &winding, mapplanes[side->planenum ^ 1].normal, mapplanes[side->planenum ^ 1].dist, 0.1 );    //CLIP_EPSILON);
				}
			} //end for
			  //VectorCopy(pc->bounds[0], brush->mins);
			  //VectorCopy(pc->bounds[1], brush->maxs);
			if ( !winding ) {
				Log_Print( "WARNING: AAS_CreateCurveBrushes: no winding\n" );
				brush->numsides = 0;
				continue;
			} //end if
			brush->original_sides[0].winding = winding;
			WindingBounds( winding, brush->mins, brush->maxs );
			for ( n = 0; n < 3; n++ )
			{
				//IDBUG: all the indexes into the mins and maxs were zero (not using i)
				if ( brush->mins[n] < -MAX_MAP_BOUNDS || brush->maxs[n] > MAX_MAP_BOUNDS ) {
					Log_Print( "entity %i, brush %i: bounds out of range\n", brush->entitynum, brush->brushnum );
					Log_Print( "brush->mins[%d] = %f, brush->maxs[%d] = %f\n", n, brush->mins[n], n, brush->maxs[n] );
					brush->numsides = 0; //remove the brush
					break;
				} //end if
				if ( brush->mins[n] > MAX_MAP_BOUNDS || brush->maxs[n] < -MAX_MAP_BOUNDS ) {
					Log_Print( "entity %i, brush %i: no visible sides on brush\n", brush->entitynum, brush->brushnum );
					Log_Print( "brush->mins[%d] = %f, brush->maxs[%d] = %f\n", n, brush->mins[n], n, brush->maxs[n] );
					brush->numsides = 0; //remove the brush
					break;
				} //end if
			} //end for
			if ( create_aas ) {
				//NOTE: brush bevels now already added
				//AddBrushBevels(brush);
				AAS_CreateMapBrushes( brush, mapent, false );
			} //end if
			else
			{
				// create windings for sides and bounds for brush
				MakeBrushWindings( brush );
				AddBrushBevels( brush );
				nummapbrushes++;
				mapent->numbrushes++;
			} //end else
		} //end for
	} //end for
	  //qprintf("\r%6d curve brushes", nummapbrushsides);//++numcurvebrushes);
	qprintf( "\r%6d curve brushes\n", numcurvebrushes );
} //end of the function AAS_CreateCurveBrushes
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void AAS_ExpandMapBrush( mapbrush_t *brush, vec3_t mins, vec3_t maxs );

void IW3_LoadMapFromFastfile( struct quakefile_s *qf ) {
	int i;
	vec3_t mins = {-1,-1,-1}, maxs = {1, 1, 1};

	Log_Print( "-- IW3_LoadMapFromBSP --\n" );
	//loaded map type
	loadedmaptype = MAPTYPE_IW3;

	Log_Print( "Loading map from %s...\n", qf->filename );
	//load the bsp file
	IW3_LoadFastfile( qf );

	//create an index from bsp planes to map planes
	//DPlanes2MapPlanes();
	//clear brush model numbers
	for ( i = 0; i < MAX_MAPFILE_BRUSHES; i++ )
		brushmodelnumbers[i] = -1;

	nummapbrushsides = 0;
	num_entities = 0;

	IW3_ParseEntities();
	//
	for ( i = 0; i < num_entities; i++ )
	{
		IW3_ParseBSPEntity( i );
	} //end for

	AAS_CreateCurveBrushes();
	//get the map mins and maxs from the world model
	ClearBounds( map_mins, map_maxs );
	for ( i = 0; i < entities[0].numbrushes; i++ )
	{
		if ( mapbrushes[i].numsides <= 0 ) {
			continue;
		}
		if ( mapbrushes[i].mins[0] > 4096 ) {
			continue;   //no valid points
		}
		AddPointToBounds( mapbrushes[i].mins, map_mins, map_maxs );
		AddPointToBounds( mapbrushes[i].maxs, map_mins, map_maxs );
	} //end for
	assert(map_mins[0] < map_maxs[0]);
	assert(map_mins[1] < map_maxs[1]);
	assert(map_mins[2] < map_maxs[2]);

	  /*/
	  for (i = 0; i < nummapbrushes; i++)
	  {
		  //if (!mapbrushes[i].original_sides) continue;
		  //AddBrushBevels(&mapbrushes[i]);
		  //AAS_ExpandMapBrush(&mapbrushes[i], mins, maxs);
	  } //end for*/
	  /*
	  for (i = 0; i < nummapbrushsides; i++)
	  {
		  Log_Write("side %d flags = %d", i, brushsides[i].flags);
	  } //end for
	  for (i = 0; i < nummapbrushes; i++)
	  {
		  Log_Write("brush contents: ");
		  PrintContents(mapbrushes[i].contents);
		  Log_Print("\n");
	  } //end for*/
} //end of the function Q3_LoadMapFromBSP



//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void IW3_ResetMapLoading( void ) {
	//reset for map loading from bsp
	memset( nodestack, 0, NODESTACKSIZE * sizeof( int ) );
	nodestackptr = NULL;
	nodestacksize = 0;
	memset( brushmodelnumbers, 0, MAX_MAPFILE_BRUSHES * sizeof( int ) );
} //end of the function Q3_ResetMapLoading


//===========================================================================
// CoD4 (iwmap 4) map writer
//
// The generic WriteMapFileSafe writes Quake style brushes which the CoD4
// Radiant can't read. Here the brush faces are rebuilt in double precision
// (the float windings of the loader only cover +-BOGUS_RANGE units) and every
// side keeps its real material.
//===========================================================================

#define IWMAP_MAX_POINTS    64
#define IWMAP_MAX_SIDES     256
#define IWMAP_BASE_SIZE     262144.0
#define IWMAP_ON_EPSILON    0.001
#define IWMAP_MIN_AREA      0.5     //faces smaller than this (square units) are dropped
#define IWMAP_MIN_TRIANGLE  16.0    //2x area of the plane point triangle
#define IWMAP_TEX_SIZE      "128 128 0 0 0 0"
#define IWMAP_LIGHTMAP      "lightmap_gray 16384 16384 0 0 0 0"

typedef struct
{
	int numpoints;
	double p[IWMAP_MAX_POINTS][3];
} iwmap_winding_t;

typedef struct
{
	double normal[3];
	double dist;
	int shadernum;
	const char *material;    //overrides shadernum when set
} iwmap_plane_t;

extern int c_writtenbrushes;     //map.c
static int iwmap_brushesskipped;
static int iwmap_placeholders;
#define IWMAP_PLACEHOLDER   "nodraw_notsolid"
static int iwmap_sidesdropped;

static double IWMap_Dot( const double *a, const double *b ) {
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static void IWMap_Cross( const double *a, const double *b, double *out ) {
	out[0] = a[1] * b[2] - a[2] * b[1];
	out[1] = a[2] * b[0] - a[0] * b[2];
	out[2] = a[0] * b[1] - a[1] * b[0];
}

static double IWMap_Normalize( double *v ) {
	double len = sqrt( IWMap_Dot( v, v ) );
	if ( len > 0 ) {
		v[0] /= len; v[1] /= len; v[2] /= len;
	}
	return len;
}

//a huge quad on the plane
static void IWMap_BaseWinding( const iwmap_plane_t *pl, iwmap_winding_t *w ) {
	double up[3] = {0, 0, 0}, right[3], org[3], d;
	int i, axis = 0;

	for ( i = 1; i < 3; i++ )
		if ( fabs( pl->normal[i] ) > fabs( pl->normal[axis] ) ) {
			axis = i;
		}
	if ( axis == 2 ) {
		up[0] = 1;
	} else { up[2] = 1;}

	d = IWMap_Dot( up, pl->normal );
	for ( i = 0; i < 3; i++ ) up[i] -= d * pl->normal[i];
	IWMap_Normalize( up );
	IWMap_Cross( up, pl->normal, right );

	for ( i = 0; i < 3; i++ )
	{
		org[i] = pl->normal[i] * pl->dist;
		up[i] *= IWMAP_BASE_SIZE;
		right[i] *= IWMAP_BASE_SIZE;
	}
	for ( i = 0; i < 3; i++ )
	{
		w->p[0][i] = org[i] - right[i] + up[i];
		w->p[1][i] = org[i] + right[i] + up[i];
		w->p[2][i] = org[i] + right[i] - up[i];
		w->p[3][i] = org[i] - right[i] - up[i];
	}
	w->numpoints = 4;
}

//keeps the part of the winding behind the plane (inside the brush)
static void IWMap_ChopWinding( iwmap_winding_t *w, const iwmap_plane_t *pl ) {
	static iwmap_winding_t out;
	double dists[IWMAP_MAX_POINTS + 1], frac;
	int sides[IWMAP_MAX_POINTS + 1];
	int i, j, front = 0;
	const double *p1, *p2;

	if ( !w->numpoints ) {
		return;
	}
	for ( i = 0; i < w->numpoints; i++ )
	{
		dists[i] = IWMap_Dot( w->p[i], pl->normal ) - pl->dist;
		if ( dists[i] > IWMAP_ON_EPSILON ) {
			sides[i] = 1; front++;
		} else if ( dists[i] < -IWMAP_ON_EPSILON ) {
			sides[i] = -1;
		} else { sides[i] = 0;}
	}
	if ( !front ) {
		return; //completely inside
	}
	dists[i] = dists[0];
	sides[i] = sides[0];

	out.numpoints = 0;
	for ( i = 0; i < w->numpoints; i++ )
	{
		if ( out.numpoints >= IWMAP_MAX_POINTS - 1 ) {
			break;
		}
		if ( sides[i] <= 0 ) {
			memcpy( out.p[out.numpoints++], w->p[i], sizeof( out.p[0] ) );
		}
		if ( sides[i] == 0 || sides[i + 1] == 0 || sides[i] == sides[i + 1] ) {
			continue;
		}
		//the edge crosses the plane, add the split point
		frac = dists[i] / ( dists[i] - dists[i + 1] );
		p1 = w->p[i];
		p2 = w->p[( i + 1 ) % w->numpoints];
		for ( j = 0; j < 3; j++ )
			out.p[out.numpoints][j] = p1[j] + frac * ( p2[j] - p1[j] );
		out.numpoints++;
	}
	*w = out;
	if ( w->numpoints < 3 ) {
		w->numpoints = 0;
	}
}

//returns twice the triangle area, normal = ( a - b ) x ( c - b )
static double IWMap_TriangleArea2( const double *a, const double *b, const double *c, double *normal ) {
	double e1[3], e2[3];
	int i;
	for ( i = 0; i < 3; i++ )
	{
		e1[i] = a[i] - b[i];
		e2[i] = c[i] - b[i];
	}
	IWMap_Cross( e1, e2, normal );
	return sqrt( IWMap_Dot( normal, normal ) );
}

static void IWMap_PrintCoord( FILE *fp, double v ) {
	char buf[64];
	int len;
	double r = floor( v + 0.5 );

	if ( fabs( v - r ) < 0.001 ) {
		if ( r == 0 ) {
			r = 0; //no "-0"
		}
		fprintf( fp, "%.0f", r );
		return;
	}
	sprintf( buf, "%.4f", v );
	//strip trailing zeros
	len = strlen( buf );
	while ( len > 0 && buf[len - 1] == '0' ) buf[--len] = 0;
	if ( len > 0 && buf[len - 1] == '.' ) {
		buf[--len] = 0;
	}
	fputs( buf, fp );
}

static void IWMap_PrintPoint( FILE *fp, const double *p ) {
	fprintf( fp, "( " );
	IWMap_PrintCoord( fp, p[0] ); fprintf( fp, " " );
	IWMap_PrintCoord( fp, p[1] ); fprintf( fp, " " );
	IWMap_PrintCoord( fp, p[2] ); fprintf( fp, " ) " );
}

//three points on the plane around the face center, 64 units apart
static void IWMap_PlanePoints( const iwmap_plane_t *pl, const iwmap_winding_t *w, double pts[3][3] ) {
	iwmap_winding_t base;
	double center[3] = {0, 0, 0}, u[3], v[3], d;
	int i, j;

	for ( i = 0; i < w->numpoints; i++ )
		for ( j = 0; j < 3; j++ )
			center[j] += w->p[i][j] / w->numpoints;
	//project the center exactly onto the plane
	d = IWMap_Dot( center, pl->normal ) - pl->dist;
	for ( j = 0; j < 3; j++ ) center[j] -= d * pl->normal[j];
	//the base winding gives two axes on the plane
	IWMap_BaseWinding( pl, &base );
	for ( j = 0; j < 3; j++ )
	{
		u[j] = base.p[1][j] - base.p[0][j];
		v[j] = base.p[3][j] - base.p[0][j];
	}
	IWMap_Normalize( u );
	IWMap_Normalize( v );
	for ( j = 0; j < 3; j++ )
	{
		pts[0][j] = center[j];
		pts[1][j] = center[j] + u[j] * 64;
		pts[2][j] = center[j] + v[j] * 64;
	}
}

static const char *IWMap_MaterialName( int shadernum ) {
	const char *name;
	if ( shadernum < 0 || shadernum >= iw3_numShaders ) {
		return "caulk";
	}
	name = iw3_dshaders[shadernum].shader;
	if ( !name[0] ) {
		return "caulk";
	}
	//the Radiant adds the wc/ prefix itself
	if ( ( name[0] == 'w' || name[0] == 'W' ) && ( name[1] == 'c' || name[1] == 'C' ) && name[2] == '/' ) {
		name += 3;
	}
	return name;
}

static qboolean IWMap_WritePlanes( FILE *fp, iwmap_plane_t *planes, int numplanes, int entnum, int srcbrushnum, int *brushnum );

//writes one brush, returns false when the brush is degenerate and was skipped
static qboolean IWMap_WriteBrush( FILE *fp, mapbrush_t *brush, vec3_t origin, int *brushnum ) {
	static iwmap_plane_t planes[IWMAP_MAX_SIDES];
	int numplanes, i, j;
	double org[3];
	plane_t *mp;
	side_t *s;

	if ( brush->numsides < 4 ) {
		return false;
	}
	org[0] = origin[0]; org[1] = origin[1]; org[2] = origin[2];
	numplanes = 0;
	for ( i = 0; i < brush->numsides && numplanes < IWMAP_MAX_SIDES; i++ )
	{
		s = brush->original_sides + i;
		if ( s->planenum < 0 || s->planenum >= nummapplanes ) {
			continue;
		}
		mp = &mapplanes[s->planenum];
		for ( j = 0; j < 3; j++ ) planes[numplanes].normal[j] = mp->normal[j];
		IWMap_Normalize( planes[numplanes].normal );
		//brush entities are stored relative to their origin
		planes[numplanes].dist = (double)mp->dist + IWMap_Dot( planes[numplanes].normal, org );
		planes[numplanes].shadernum = s->shadernum;
		planes[numplanes].material = NULL;
		numplanes++;
	}
	return IWMap_WritePlanes( fp, planes, numplanes, brush->entitynum, brush->brushnum, brushnum );
}

//builds the faces of a convex brush from its planes and writes it
static qboolean IWMap_WritePlanes( FILE *fp, iwmap_plane_t *planes, int numplanes, int entnum, int srcbrushnum, int *brushnum ) {
	static iwmap_winding_t windings[IWMAP_MAX_SIDES];
	int i, j, k, a, b, c, best[3], numvalid;
	double area, bestarea, n[3], pts[3][3];
	iwmap_winding_t *w;

	//build the face of every side, sides without a face are redundant planes
	numvalid = 0;
	for ( i = 0; i < numplanes; i++ )
	{
		w = &windings[i];
		IWMap_BaseWinding( &planes[i], w );
		for ( j = 0; j < numplanes && w->numpoints; j++ )
		{
			if ( j != i ) {
				IWMap_ChopWinding( w, &planes[j] );
			}
		}
		if ( w->numpoints ) {
			area = 0;
			for ( k = 2; k < w->numpoints; k++ )
				area += IWMap_TriangleArea2( w->p[0], w->p[k - 1], w->p[k], n );
			if ( area * 0.5 < IWMAP_MIN_AREA ) {
				w->numpoints = 0;
			}
		}
		if ( w->numpoints ) {
			numvalid++;
		} else { iwmap_sidesdropped++;}
	}
	if ( numvalid < 4 ) {
		Log_Write( "IW3 map: entity %d brush %d skipped, only %d valid sides\n", entnum, srcbrushnum, numvalid );
		return false;
	}

	fprintf( fp, "// brush %d\n{\n", *brushnum );
	for ( i = 0; i < numplanes; i++ )
	{
		w = &windings[i];
		if ( !w->numpoints ) {
			continue;
		}
		//pick the three face points spanning the biggest triangle
		bestarea = -1;
		best[0] = 0; best[1] = 1; best[2] = 2;
		for ( a = 0; a < w->numpoints; a++ )
			for ( b = a + 1; b < w->numpoints; b++ )
				for ( c = b + 1; c < w->numpoints; c++ )
				{
					area = IWMap_TriangleArea2( w->p[a], w->p[b], w->p[c], n );
					if ( area > bestarea ) {
						bestarea = area;
						best[0] = a; best[1] = b; best[2] = c;
					}
				}
		memcpy( pts[0], w->p[best[0]], sizeof( pts[0] ) );
		memcpy( pts[1], w->p[best[1]], sizeof( pts[1] ) );
		memcpy( pts[2], w->p[best[2]], sizeof( pts[2] ) );
		if ( bestarea < IWMAP_MIN_TRIANGLE ) {
			//thin face, the corners don't define the plane well enough,
			//use points spread out on the plane around the face center instead
			IWMap_PlanePoints( &planes[i], w, pts );
		}
		//( p0 - p1 ) x ( p2 - p1 ) has to point out of the brush
		IWMap_TriangleArea2( pts[0], pts[1], pts[2], n );
		if ( IWMap_Dot( n, planes[i].normal ) < 0 ) {
			memcpy( n, pts[0], sizeof( n ) );
			memcpy( pts[0], pts[2], sizeof( pts[0] ) );
			memcpy( pts[2], n, sizeof( pts[2] ) );
		}
		fprintf( fp, " " );
		IWMap_PrintPoint( fp, pts[0] );
		IWMap_PrintPoint( fp, pts[1] );
		IWMap_PrintPoint( fp, pts[2] );
		fprintf( fp, "%s " IWMAP_TEX_SIZE " " IWMAP_LIGHTMAP "\n", planes[i].material ? planes[i].material : IWMap_MaterialName( planes[i].shadernum ) );
	}
	fprintf( fp, "}\n" );
	( *brushnum )++;
	c_writtenbrushes++;
	return true;
}

//entities which Radiant only accepts with at least one brush
static qboolean IWMap_IsBrushEntity( entity_t *mapent ) {
	char *classname = ValueForKey( mapent, "classname" );

	if ( mapent->modelnum > 0 ) {
		return true;
	}
	if ( !Q_strncasecmp( classname, "trigger_", 8 ) ) {
		//these two are point entities with a radius
		return Q_stricmp( classname, "trigger_radius" ) && Q_stricmp( classname, "trigger_disk" );
	}
	return !Q_stricmp( classname, "script_brushmodel" ) || !Q_strncasecmp( classname, "func_", 5 );
}

//some brush models have only visible geometry (no collision brushes in the
//fastfile), write a non solid box with the size of the model instead
static void IWMap_WritePlaceholderBrush( FILE *fp, entity_t *mapent, vec3_t origin, int *brushnum ) {
	iwmap_plane_t planes[6];
	double mins[3], maxs[3];
	int i, valid = false;

	if ( mapent->modelnum > 0 && mapent->modelnum < iw3_nummodels ) {
		q3_dmodel_t *m = &iw3_dmodels[mapent->modelnum];
		valid = true;
		for ( i = 0; i < 3; i++ )
		{
			//the loader spreads the bounds by one unit
			mins[i] = m->mins[i] - 1;
			maxs[i] = m->maxs[i] + 1;
			if ( !( maxs[i] - mins[i] >= 1 && maxs[i] - mins[i] < 16384 ) ) {
				valid = false;
			}
		}
		Log_Write( "IW3 map: entity %s model *%d bounds %g %g %g - %g %g %g origin %g %g %g\n",
				   ValueForKey( mapent, "classname" ), mapent->modelnum,
				   mins[0], mins[1], mins[2], maxs[0], maxs[1], maxs[2], origin[0], origin[1], origin[2] );
	}
	for ( i = 0; i < 3; i++ )
	{
		if ( valid ) {
			//model bounds are relative to the entity origin
			mins[i] += origin[i];
			maxs[i] += origin[i];
		} else {
			double o = 0;
			GetVectorForKey( mapent, "origin", origin );
			o = origin[i];
			mins[i] = o - 8;
			maxs[i] = o + 8;
		}
	}
	for ( i = 0; i < 3; i++ )
	{
		memset( &planes[i * 2], 0, sizeof( planes[0] ) * 2 );
		planes[i * 2].normal[i] = 1;
		planes[i * 2].dist = maxs[i];
		planes[i * 2 + 1].normal[i] = -1;
		planes[i * 2 + 1].dist = -mins[i];
		planes[i * 2].material = planes[i * 2 + 1].material = IWMAP_PLACEHOLDER;
	}
	IWMap_WritePlanes( fp, planes, 6, mapent - entities, -1, brushnum );
}

qboolean IW3_WriteMapFileSafe( FILE *fp ) {
	int i, bn, brushnum, outent;
	epair_t *ep, *ep2;
	entity_t *mapent;
	vec3_t origin;

	iwmap_brushesskipped = 0;
	iwmap_sidesdropped = 0;
	iwmap_placeholders = 0;

	if ( fprintf( fp, "iwmap 4\n\"000_Global\" flags  active\n\"The Map\" flags \n" ) < 0 ) {
		return false;
	}

	outent = 0;
	for ( i = 0; i < num_entities; i++ )
	{
		mapent = &entities[i];
		if ( !mapent->epairs ) {
			continue;
		}
		fprintf( fp, "// entity %d\n{\n", outent++ );
		for ( ep = mapent->epairs; ep; ep = ep->next )
		{
			//the entity string of some fastfiles has every key twice
			for ( ep2 = mapent->epairs; ep2 != ep; ep2 = ep2->next )
				if ( !strcmp( ep2->key, ep->key ) ) {
					break;
				}
			if ( ep2 != ep ) {
				continue;
			}
			//don't write BSP model numbers
			if ( !strcmp( ep->key, "model" ) && ep->value[0] == '*' ) {
				continue;
			}
			fprintf( fp, "\"%s\" \"%s\"\n", ep->key, ep->value );
		}
		VectorClear( origin );
		if ( mapent->modelnum > 0 ) {
			GetVectorForKey( mapent, "origin", origin );
		}
		brushnum = 0;
		for ( bn = 0; bn < nummapbrushes; bn++ )
		{
			if ( mapbrushes[bn].entitynum != i ) {
				continue;
			}
			if ( !IWMap_WriteBrush( fp, &mapbrushes[bn], origin, &brushnum ) ) {
				if ( mapbrushes[bn].numsides > 0 ) {
					iwmap_brushesskipped++;
				}
			}
		}
		//Radiant asserts on a brush entity without brushes
		if ( !brushnum && IWMap_IsBrushEntity( mapent ) ) {
			IWMap_WritePlaceholderBrush( fp, mapent, origin, &brushnum );
			iwmap_placeholders++;
		}
		if ( fprintf( fp, "}\n" ) < 0 ) {
			return false;
		}
	}
	Log_Print( "IW3 map: %d degenerate brushes skipped, %d redundant brush sides removed\n",
			   iwmap_brushesskipped, iwmap_sidesdropped );
	if ( iwmap_placeholders ) {
		Log_Print( "IW3 map: %d brush entities had no collision brushes, wrote a %s placeholder brush\n",
				   iwmap_placeholders, IWMAP_PLACEHOLDER );
	}
	return true;
}
