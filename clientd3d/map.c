// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * map.c:  This file deals with drawing the room map.
 *
 * The map is a view of the room from above, showing only walls that
 * the user has seen.  Each wall in the BSP tree carries a flag that
 * is set when the wall is drawn; this is how we tell which walls the
 * user has seen.
 *
 * Each wall also has a flag which allows it to be shown even when it
 * hasn't been seen, or to never show it.
 */

#include "client.h"

#define MAP_WALL_THICKNESS 1
#define MAP_PLAYER_THICKNESS 4
#define MAP_OBJECT_THICKNESS 2
#define MAP_BOSS_THICKNESS 3
#define MAP_AGGRO_SELF_THICKNESS 5
#define MAP_AGGRO_OTHER_THICKNESS 4
#define MAP_BOSS_AGGRO_SELF_THICKNESS 6
#define MAP_BOSS_AGGRO_OTHER_THICKNESS 5

#define MAP_WALL_COLOR          PALETTERGB(0, 0, 0)
#define MAP_PLAYER_COLOR        PALETTERGB(0, 0, 255)
#define MAP_PLAYER_FRONT_COLOR  PALETTERGB(0, 0, 0)      // Pixel at front of player
#define MAP_OBJECT_COLOR        PALETTERGB(255, 0, 0)    // Red
#define MAP_MINION_COLOR        PALETTERGB(0,200,0)      // Green
#define MAP_MINION_OTH_COLOR    PALETTERGB(70,5,130)     // Purple
#define MAP_FRIEND_COLOR        PALETTERGB(0, 255, 120)  // Green with blue tint
#define MAP_ENEMY_COLOR         PALETTERGB(255, 0, 0)    // Red
#define MAP_AGGRO_SELF_COLOR    PALETTERGB(0,0,0)        // Black
#define MAP_AGGRO_OTHER_COLOR   PALETTERGB(255,255,255)  // White
#define MAP_MERCENARY_COLOR     PALETTERGB(255, 169, 27) // Gold
#define MAP_GUILDMATE_COLOR     PALETTERGB(255, 255, 0)  // Yellow
#define MAP_BUILDGRP_COLOR      PALETTERGB(0, 255, 0)    // Bright Green
#define MAP_COLOR_NOPVP         PALETTERGB(255,255,255)  // White
#define MAP_NPC_COLOR           PALETTERGB(0, 0, 0)      // Black
#define MAP_TEMPSAFE_COLOR      PALETTERGB(0,170,255)    // Cyan
#define MAP_MINIBOSS_COLOR      PALETTERGB(160, 66, 194) // Purple
#define MAP_BOSS_COLOR          PALETTERGB(127, 0, 0)    // Dark Red
#define MAP_RARE_ITEM_COLOR     PALETTERGB(237, 255, 9)  // Orange
#define MAP_MOB_QUEST_COLOR     PALETTERGB(179,0,179)    // Brighter purple

#define MAP_OBJECT_RADIUS (FINENESS / 4)  // Radius of circle drawn for an object

#define MAP_ZOOM_INCREMENT 0.05       // Amount to change zoom factor per user command
#define MAP_ZOOM_DELAY     30       // # of milliseconds between zooming in by INCREMENT
#define MAP_ZOOM_MINZOOM   0.5       // Farthest we can zoom out
#define MAP_ZOOM_MAXZOOM   8.0       // Closest we can zoom in

#define MAP_OBJECT_DISTANCE (7 * FINENESS) // Draw all object closer than this to player

static HBRUSH hObjectBrush, hPlayerBrush, hNullBrush, hMinionBrush, hNoPVPBrush,
              hMinionOtherBrush, hNpcBrush, hTempsafeBrush, hItemBrush, hAggroSelfBrush,
              hAggroOtherBrush, hMercenaryBrush, hMobQuestBrush;
static HPEN hWallPen, hPlayerPen, hObjectPen, hMinionPen, hMinionOtherPen,
            hMinibossPen, hBossPen, hItemPen, hFriendPen, hEnemyPen, hAggroSelfPen,
            hGuildmatePen, hBuilderPen, hNpcPen, hTempsafePen, hAggroOtherPen,
            hNoPVPPen, hBossAggroSelfPen, hBossAggroOtherPen, hMercenaryPen,
            hMobQuestPen;

static float zoom;              // Factor to zoom in on map

static BOOL bDrawBackgroundStatic = False;

// To go from room coordinates (rx, xy) to screen coordinates (sx, sy):
// sx = xoffset + rx * scale
// sy = yoffset + ry * scale
static float scale;
static float scaleMiniMap;
static int   xoffset, yoffset;
static int   xoffsetMiniMap, yoffsetMiniMap;

static RawBitmap annotation;             // Annotation bitmap
static RawBitmap map_bkgnd;              // Background bitmap for map area

extern player_info player;
extern room_type current_room;
extern WallData *lastBlockingWall;

struct map_wall_cache_t
{
   POINT p0;
   POINT p1;
};

static BOOL fMapCacheValid = FALSE;
static struct map_wall_cache_t *pMapWalls = NULL;
static float mapCacheScale = 0.0;
static int mapNumCacheWalls = 0;

/* local function prototypes */
static void MapDrawMiniMapWalls(HDC hdc, int x, int y, room_type *room);
static void MapDrawWall(HDC hdc, int x, int y, float scale, WallData *wall);
static void MapDrawPlayer(HDC hdc, int x, int y, float scale, int minimapflags);
static void MapDrawObjects(HDC hdc, list_type objects, int x, int y, float scale);
static inline void DrawMinimapDot(HDC hdc, HPEN pen, HBRUSH brush, float radius, float x, float y);
static void DrawMinimapStar(HDC hdc, HPEN pen, HBRUSH brush, float size, float x, float y);
static void MapDrawWalls(HDC hdc, int x, int y, float scale, room_type *room);
static void MapDrawAnnotations( HDC hdc, MapAnnotation *annotations, int x, int y, float scaleToUse, Bool bMiniMap );

void MapSetWallPositions(room_type *room, float scale, int numWalls)
{
   int i;
   WallData *wall = room->walls;
   struct map_wall_cache_t *pMap;

   if (numWalls != mapNumCacheWalls)
   {
      if (pMapWalls)
         SafeFree(pMapWalls);
      pMapWalls = NULL;
   }
   if (NULL == pMapWalls)
   {
      pMapWalls = (struct map_wall_cache_t *)
        SafeMalloc(numWalls * sizeof(struct map_wall_cache_t));
      mapNumCacheWalls = numWalls;
   }
   pMap = pMapWalls;
   if (pMap)
   {
      for (i = 0; i < numWalls; i++,pMap++,wall++)
      {
    pMap->p0.x = (int)(((float)wall->x0) * scale);
    pMap->p0.y = (int)(((float)wall->y0) * scale);
    pMap->p1.x = (int)(((float)wall->x1) * scale);
    pMap->p1.y = (int)(((float)wall->y1) * scale);
      }
      mapCacheScale = scale;
   }
}

/*****************************************************************************/
/*
 * MapInitialize:  Create graphics objects for drawing map.
 */
void MapInitialize(void)
{
   LOGBRUSH logBrush;

   ZeroMemory(&logBrush,sizeof(logBrush));
   logBrush.lbStyle = BS_HOLLOW;

   hWallPen = CreatePen(PS_SOLID, MAP_WALL_THICKNESS, MAP_WALL_COLOR);
   hPlayerPen = CreatePen(PS_SOLID, MAP_OBJECT_THICKNESS, MAP_PLAYER_COLOR);
   hObjectPen = CreatePen(PS_SOLID, MAP_OBJECT_THICKNESS, MAP_OBJECT_COLOR);
   hFriendPen = CreatePen(PS_SOLID, MAP_PLAYER_THICKNESS, MAP_FRIEND_COLOR);
   hEnemyPen = CreatePen(PS_SOLID, MAP_PLAYER_THICKNESS, MAP_ENEMY_COLOR);
   hAggroSelfPen = CreatePen(PS_SOLID, MAP_AGGRO_SELF_THICKNESS, MAP_AGGRO_SELF_COLOR);
   hAggroOtherPen = CreatePen(PS_SOLID, MAP_AGGRO_OTHER_THICKNESS, MAP_AGGRO_OTHER_COLOR);
   hMercenaryPen = CreatePen(PS_SOLID, MAP_OBJECT_THICKNESS, MAP_MERCENARY_COLOR);
   hBossAggroSelfPen = CreatePen(PS_SOLID, MAP_BOSS_AGGRO_SELF_THICKNESS, MAP_AGGRO_SELF_COLOR);
   hBossAggroOtherPen = CreatePen(PS_SOLID, MAP_BOSS_AGGRO_OTHER_THICKNESS, MAP_AGGRO_OTHER_COLOR);
   hGuildmatePen = CreatePen(PS_SOLID, MAP_PLAYER_THICKNESS, MAP_GUILDMATE_COLOR);
   hMinionPen = CreatePen(PS_SOLID, MAP_OBJECT_THICKNESS, MAP_MINION_COLOR);
   hMinionOtherPen = CreatePen(PS_SOLID, MAP_OBJECT_THICKNESS, MAP_MINION_OTH_COLOR);
   hBuilderPen = CreatePen(PS_SOLID, MAP_OBJECT_THICKNESS, MAP_BUILDGRP_COLOR);
   hNoPVPPen = CreatePen(PS_SOLID, MAP_OBJECT_THICKNESS, MAP_COLOR_NOPVP);
   hNpcPen = CreatePen(PS_SOLID, MAP_OBJECT_THICKNESS, MAP_NPC_COLOR);
   hTempsafePen = CreatePen(PS_SOLID, MAP_OBJECT_THICKNESS, MAP_TEMPSAFE_COLOR);
   hMinibossPen = CreatePen(PS_SOLID, MAP_BOSS_THICKNESS, MAP_MINIBOSS_COLOR);
   hBossPen = CreatePen(PS_SOLID, MAP_BOSS_THICKNESS, MAP_BOSS_COLOR);
   hItemPen = CreatePen(PS_SOLID, MAP_OBJECT_THICKNESS, MAP_RARE_ITEM_COLOR);
   hMobQuestPen = CreatePen(PS_SOLID, MAP_OBJECT_THICKNESS, MAP_MOB_QUEST_COLOR);

   hNpcBrush = CreateSolidBrush(MAP_NPC_COLOR);
   hMinionOtherBrush = CreateSolidBrush(MAP_MINION_OTH_COLOR);
   hMinionBrush = CreateSolidBrush(MAP_MINION_COLOR);
   hObjectBrush = CreateSolidBrush(MAP_OBJECT_COLOR);
   hAggroSelfBrush = CreateSolidBrush(MAP_AGGRO_SELF_COLOR);
   hAggroOtherBrush = CreateSolidBrush(MAP_AGGRO_OTHER_COLOR);
   hMercenaryBrush = CreateSolidBrush(MAP_MERCENARY_COLOR);
   hPlayerBrush = CreateSolidBrush(MAP_PLAYER_COLOR);
   hTempsafeBrush = CreateSolidBrush(MAP_TEMPSAFE_COLOR);
   hNoPVPBrush = CreateSolidBrush(MAP_COLOR_NOPVP);
   hItemBrush = CreateSolidBrush(MAP_RARE_ITEM_COLOR);
   hMobQuestBrush = CreateSolidBrush(MAP_MOB_QUEST_COLOR);
   hNullBrush = CreateBrushIndirect(&logBrush);

   zoom = (float) 1.0;

   // Load map annotation bitmap
   if (!GetBitmapResourceInfo(hInst, IDB_ANNOTATE, &annotation))
     debug(("MapInitialize couldn't load map annotation bitmap\n"));

   // Load map background
   if (!GetBitmapResourceInfo(hInst, IDB_MAPBKGND, &map_bkgnd))
     debug(("MapInitialize couldn't load map background bitmap\n"));

   fMapCacheValid = FALSE;
   if (pMapWalls)
      SafeFree(pMapWalls);
   pMapWalls = NULL;
}
/*****************************************************************************/
/*
 * MapClose:  Free map graphics objects & memory
 */
void MapClose(void)
{
   DeleteObject(hWallPen);
   DeleteObject(hPlayerPen);
   DeleteObject(hObjectPen);
   DeleteObject(hFriendPen);
   DeleteObject(hEnemyPen);
   DeleteObject(hAggroSelfPen);
   DeleteObject(hAggroOtherPen);
   DeleteObject(hMercenaryPen);
   DeleteObject(hBossAggroSelfPen);
   DeleteObject(hBossAggroOtherPen);
   DeleteObject(hGuildmatePen);
   DeleteObject(hMinionPen);
   DeleteObject(hMinionOtherPen);
   DeleteObject(hBuilderPen);
   DeleteObject(hNoPVPPen);
   DeleteObject(hNpcPen);
   DeleteObject(hTempsafePen);
   DeleteObject(hMinibossPen);
   DeleteObject(hBossPen);
   DeleteObject(hItemPen);
   DeleteObject(hMinionOtherBrush);
   DeleteObject(hMinionBrush);
   DeleteObject(hObjectBrush);
   DeleteObject(hAggroSelfBrush);
   DeleteObject(hAggroOtherBrush);
   DeleteObject(hPlayerBrush);
   DeleteObject(hNullBrush);
   DeleteObject(hNpcBrush);
   DeleteObject(hTempsafeBrush);
   DeleteObject(hNoPVPBrush);
   DeleteObject(hItemBrush);
   DeleteObject(hMercenaryBrush);
   DeleteObject(hMobQuestPen);
   DeleteObject(hMobQuestBrush);

   if (pMapWalls)
      SafeFree(pMapWalls);
   pMapWalls = NULL;
}
/*****************************************************************************/
/*
 * MapDraw:  Draw map of the given room on the given area of hdc.
 *   bits points to the bitmap in hdc.
 *   width gives the width of the bitmap in hdc.
 */
void MapDraw( HDC hdc, BYTE *bits, AREA *area, room_type *room, int width, Bool bMiniMap )
{
   RECT rect;
   AREA view;

   AreaToRect(area, &rect);
   CopyCurrentView(&view);

   DrawWindowBackgroundMem(&map_bkgnd, bits, &rect, width, view.x, view.y);
   if( !bMiniMap )
      DrawWindowBackgroundMem(&map_bkgnd, bits, &rect, width, (int)( player.x * scale ), (int)( player.y * scale ) );
   else
      if( bDrawBackgroundStatic )
      {
    DrawWindowBackgroundMem(&map_bkgnd, bits, &rect, width, view.x, view.y);
    bDrawBackgroundStatic = False;
      }
   else
      DrawWindowBackgroundMem(&map_bkgnd, bits, &rect, width, (int)( player.x * scaleMiniMap ), (int)( player.y * scaleMiniMap ) );

      if (config.drawmap)
      {
    HPEN hOldPen = (HPEN) SelectObject(hdc, hWallPen);
    HBRUSH hOldBrush = (HBRUSH) SelectObject(hdc, GetStockObject(NULL_BRUSH));
    if( !bMiniMap )
    {
       scale = ((float) area->cx) / room->width;
       scale = min(scale, ((float) area->cy) / room->height);
       scale *= zoom;

       // Center map on player
       xoffset = area->x + area->cx / 2 - (int) (player.x * scale);
       yoffset = area->y + area->cy / 2 - (int) (player.y * scale);

       MapDrawWalls(hdc, xoffset, yoffset, scale, room);
       if (config.map_annotations)
          MapDrawAnnotations(hdc, room->annotations, xoffset, yoffset, scale, FALSE );
       MapDrawObjects(hdc, room->contents, xoffset, yoffset, scale);
    }
    else
    {
       float cacheDiff;
       scaleMiniMap = ((float) area->cx) / room->width;
       scaleMiniMap = min( scaleMiniMap, ((float) area->cy) / room->height );
       scaleMiniMap *= zoom;

       cacheDiff = scaleMiniMap - mapCacheScale;
       if ((cacheDiff >= MAP_ZOOM_INCREMENT) || (cacheDiff <= -MAP_ZOOM_INCREMENT))
          fMapCacheValid = FALSE;

       if (mapNumCacheWalls != room->num_walls)
          fMapCacheValid = FALSE;

       if (NULL == pMapWalls) 
          fMapCacheValid = FALSE;

       // Center map on player
       xoffsetMiniMap = area->x + area->cx / 2 - (int) (player.x * scaleMiniMap);
       yoffsetMiniMap = area->y + area->cy / 2 - (int) (player.y * scaleMiniMap);

       if (!fMapCacheValid)
       {
          MapSetWallPositions(room, scaleMiniMap, room->num_walls);
          mapCacheScale = scaleMiniMap;
          fMapCacheValid = TRUE;
       }
       if (pMapWalls) 
          MapDrawMiniMapWalls(hdc, xoffsetMiniMap, yoffsetMiniMap, room);
       else
          MapDrawWalls(hdc, xoffsetMiniMap, yoffsetMiniMap, scaleMiniMap, room);
       if (config.map_annotations)
          MapDrawAnnotations( hdc, room->annotations, xoffsetMiniMap, yoffsetMiniMap, scaleMiniMap, TRUE );
       MapDrawObjects(hdc, room->contents, xoffsetMiniMap, yoffsetMiniMap, scaleMiniMap);
    }
    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBrush);
   }
   GdiFlush();
}
/*****************************************************************************/
/*
 * MapDrawMiniMapWalls:  Draw minimap view of walls contained in given room.
 *   Walls are drawn on hdc, with (x, y) as the upper-left corner
 *   of the drawing area.  Wall coordinates are scaled by the MapSetWallPositions
 *   function.
 */
void MapDrawMiniMapWalls(HDC hdc, int x, int y, room_type *room)
{
   int i;
   struct map_wall_cache_t *pMap = pMapWalls;
   WallData *wall = room->walls;

   //assert(room->num_walls == mapNumCacheWalls);
   for (i = 0; i < mapNumCacheWalls; i++,pMap++,wall++)
   {
      Sidedef *sidedef = wall->pos_sidedef;
   
     // try other side if null
     if (sidedef == NULL)
        sidedef = wall->neg_sidedef;
      
     // skip if still null or flagged to never show up
     if (sidedef == NULL || sidedef->flags & WF_MAP_NEVER)
        continue;

      // draw highlighted
     if ((config.showMapBlocking && lastBlockingWall == wall) ||
        (config.showUnseenWalls && !(wall->seen & SR_SEEN)))
      {
        SelectObject(hdc, GetStockObject(WHITE_PEN));
       MoveToEx(hdc, x + pMap->p0.x, y + pMap->p0.y, NULL);
        LineTo(hdc, x + pMap->p1.x, y + pMap->p1.y);
        SelectObject(hdc, hWallPen);
     }
     else
     {
        MoveToEx(hdc, x + pMap->p0.x, y + pMap->p0.y, NULL);
        LineTo(hdc, x + pMap->p1.x, y + pMap->p1.y);
     }
   }
}

/*****************************************************************************/
/*
 * MapDrawWalls:  Draw map view of walls contained in given room.
 *   Walls are drawn on hdc, with (x, y) as the upper-left corner
 *   of the drawing area.  Wall coordinates are scaled by the given factor.
 */
void MapDrawWalls(HDC hdc, int x, int y, float scale, room_type *room)
{
   WallData *wall;
   int i;
   Sidedef *sidedef;

   for (i = 0; i < room->num_walls; i++)
   {
      wall = &room->walls[i];
      sidedef = wall->pos_sidedef;
      
      if (sidedef == NULL)
        sidedef = wall->neg_sidedef;
      
     if (sidedef == NULL || sidedef->flags & WF_MAP_NEVER)
        continue;

      MapDrawWall(hdc, x, y, scale, wall);
   }
}
/*****************************************************************************/
/*
 * MapDrawWall:  Draw map view of given wall.  Wall is drawn on hdc, with (x, y) 
 *   as the upper-left corner of the drawing area.  
 *   Wall coordinates are scaled by the given factor.
 */
void MapDrawWall(HDC hdc, int x, int y, float scale, WallData *wall)
{
   POINT p;

   p.x = (int) (wall->x0 * scale);
   p.y = (int) (wall->y0 * scale);
   MoveToEx(hdc, x + p.x, y + p.y, NULL);

   p.x = (int) (wall->x1 * scale);
   p.y = (int) (wall->y1 * scale);
   LineTo(hdc, x + p.x, y + p.y);
}
/*****************************************************************************/
/*
 * MapDrawObjects:  Draw a dot for each object in list, skipping the player.
 *   (x, y) is the upper-left corner of the drawing area on hdc.
 */
void MapDrawObjects(HDC hdc, list_type objects, int x, int y, float scale)
{
   list_type l;
   float radius, new_x, new_y;
   int dx, dy;
   static int mapObjectDistanceShiftAndSquare = (MAP_OBJECT_DISTANCE >> 4) * (MAP_OBJECT_DISTANCE >> 4);

   radius = max(1, MAP_OBJECT_RADIUS * scale);

   for (l = objects; l != NULL; l = l->next)
   {
      room_contents_node *r = (room_contents_node *) (l->data);

      if (r->obj.id == player.id)
      {
         MapDrawPlayer(hdc, x, y, scale, r->obj.minimapflags);
         continue;
      }

      // Skip invisible objects and anything with minimapflags of 0.
      if ((r->obj.drawingtype == DRAWFX_INVISIBLE)
         || (r->obj.minimapflags == MM_NONE))
         continue;

      // Only draw monsters that are visible, or within a certain range.
      // Draw NPC dots no matter where they are.
      dx = (r->motion.x - player.x) >> 4;
      dy = (r->motion.y - player.y) >> 4;
      if (((dx * dx + dy * dy) > mapObjectDistanceShiftAndSquare)
           && (!r->visible && !config.showUnseenMonsters)
           && (r->obj.flags & MM_MONSTER))
          continue;

      new_x = x + (r->motion.x * scale);
      new_y = y + (r->motion.y * scale);

      // Draw rings around players
      if (r->obj.flags & OF_PLAYER)
      {
         // Builder group?
         if (r->obj.minimapflags & MM_BUILDER_GROUP)
            DrawMinimapDot(hdc, hBuilderPen, hPlayerBrush, 2.6f * radius, new_x, new_y);
         // Friend?
         if (r->obj.minimapflags & MM_FRIEND)
            DrawMinimapDot(hdc, hFriendPen, hPlayerBrush, 1.6f * radius, new_x, new_y);
         // Enemy?
         if (r->obj.minimapflags & MM_ENEMY)
            DrawMinimapDot(hdc, hEnemyPen, hPlayerBrush, 1.6f * radius, new_x, new_y);
         // Guildmate?
         if (r->obj.minimapflags & MM_GUILDMATE)
            DrawMinimapDot(hdc, hGuildmatePen, hPlayerBrush, 1.6f * radius, new_x, new_y);
      }

      // Draw rings for quest NPCs
      if (r->obj.minimapflags & MM_NPCCURRENTQUEST)
         DrawMinimapDot(hdc, hFriendPen, hNpcBrush, 1.6f * radius, new_x, new_y);
      else if (r->obj.minimapflags & MM_NPCHASQUEST)
         DrawMinimapDot(hdc, hGuildmatePen, hNpcBrush, 1.6f * radius, new_x, new_y);

      /* Draw middle of player dot in a different color. If a monster
         is a minion or NPC then color it appropriately, otherwise it
         gets the standard red dot. */
      if (r->obj.minimapflags & MM_PLAYER)
         DrawMinimapDot(hdc, hPlayerPen, hPlayerBrush, radius, new_x, new_y);
      else if (r->obj.minimapflags & MM_TEMPSAFE)
         DrawMinimapDot(hdc, hTempsafePen, hTempsafeBrush, radius, new_x, new_y);
      else if (r->obj.minimapflags & MM_NO_PVP)
         DrawMinimapDot(hdc, hNoPVPPen, hNoPVPBrush, radius, new_x, new_y);
      else if (r->obj.minimapflags & MM_MINION_SELF)
         DrawMinimapDot(hdc, hMinionPen, hMinionBrush, radius, new_x, new_y);
      else if (r->obj.minimapflags & MM_MINION_OTHER)
         DrawMinimapDot(hdc, hMinionOtherPen, hMinionOtherBrush, radius, new_x, new_y);
      else if (r->obj.minimapflags & MM_NPC)
         DrawMinimapDot(hdc, hNpcPen, hNpcBrush, radius, new_x, new_y);
      else if (r->obj.minimapflags & MM_RARE_ITEM)
         DrawMinimapStar(hdc, hItemPen, hItemBrush, radius * 3.0f, new_x, new_y);
      else if (r->obj.minimapflags & MM_MOBKILLQUEST)
      {
         // Draw a ring around monsters that target us or others.
         if (r->obj.minimapflags & MM_AGGRO_SELF)
            DrawMinimapDot(hdc, hAggroSelfPen, hAggroSelfBrush, radius * 2.1f, new_x, new_y);
         else if (r->obj.minimapflags & MM_AGGRO_OTHER)
            DrawMinimapDot(hdc, hAggroOtherPen, hAggroOtherBrush, radius * 2.1f, new_x, new_y);
         DrawMinimapDot(hdc, hMobQuestPen, hMobQuestBrush, radius * 2.1f, new_x, new_y);
         DrawMinimapDot(hdc, hObjectPen, hObjectBrush, radius, new_x, new_y);
      }
      else if (r->obj.minimapflags & MM_MINIBOSS)
      {
         // Draw a ring around monsters that target us or others.
         if (r->obj.minimapflags & MM_AGGRO_SELF)
            DrawMinimapDot(hdc, hAggroSelfPen, hAggroSelfBrush, radius * 2.1f, new_x, new_y);
         else if (r->obj.minimapflags & MM_AGGRO_OTHER)
            DrawMinimapDot(hdc, hAggroOtherPen, hAggroOtherBrush, radius * 2.1f, new_x, new_y);
         DrawMinimapDot(hdc, hMinibossPen, hObjectBrush, radius * 2.1f, new_x, new_y);
         DrawMinimapDot(hdc, hObjectPen, hObjectBrush, radius * 1.2f, new_x, new_y);
      }
      else if (r->obj.minimapflags & MM_BOSS)
      {
         // Draw a ring around monsters that target us or others.
         if (r->obj.minimapflags & MM_AGGRO_SELF)
            DrawMinimapDot(hdc, hAggroSelfPen, hAggroSelfBrush, radius * 2.6f, new_x, new_y);
         else if (r->obj.minimapflags & MM_AGGRO_OTHER)
            DrawMinimapDot(hdc, hAggroOtherPen, hAggroOtherBrush, radius * 2.6f, new_x, new_y);
         DrawMinimapDot(hdc, hBossPen, hObjectBrush, radius * 2.6f, new_x, new_y);
         DrawMinimapDot(hdc, hObjectPen, hObjectBrush, radius * 1.6f, new_x, new_y);
      }
      else if (r->obj.minimapflags & MM_MONSTER)
      {
         // Draw a ring around monsters that target us or others.
         if (r->obj.minimapflags & MM_AGGRO_SELF)
            DrawMinimapDot(hdc, hAggroSelfPen, hAggroSelfBrush, radius, new_x, new_y);
         else if (r->obj.minimapflags & MM_AGGRO_OTHER)
            DrawMinimapDot(hdc, hAggroOtherPen, hAggroOtherBrush, radius, new_x, new_y);

         // Draw a mercenary dot if the mob is our mercenary, otherwise draw a regular red dot.
         if (r->obj.minimapflags & MM_MERCENARY)
            DrawMinimapDot(hdc, hMercenaryPen, hMercenaryBrush, radius, new_x, new_y);
         else
            DrawMinimapDot(hdc, hObjectPen, hObjectBrush, radius, new_x, new_y);
      }
   }
}

static inline void DrawMinimapDot(HDC hdc, HPEN pen, HBRUSH brush, float radius, float x, float y)
{
   SelectObject(hdc, pen);
   SelectObject(hdc, brush);
   Ellipse(hdc, (int)(x - radius), (int)(y - radius), (int)(x + radius), (int)(y + radius));
}

static void DrawMinimapStar(HDC hdc, HPEN pen, HBRUSH brush, float size, float x, float y)
{
   float alpha = PITWICE / 10;
   POINT p[11];

   SelectObject(hdc, pen);
   SelectObject(hdc, brush);

   // Build points
   for (int i = 0; i < 11; ++i)
   {
      float r = size * (i % 2 + 1) / 2.0f;
      float omega = alpha * i;
      p[i].x = (LONG)((r * sin(omega)) + x);
      p[i].y = (LONG)((r * cos(omega)) + y);
   }

   Polygon(hdc, p, 11);
}

/*****************************************************************************/
/*
 * MapDrawPlayer:  Draw a line on the map indicating the player's position.
 *   (x, y) is the upper-left corner of the drawing area on hdc. Draws a
 *   light-blue line for MM_TEMPSAFE, otherwise dark-blue standard player line.
 */
void MapDrawPlayer(HDC hdc, int x, int y, float scale, int minimapflags)
{
   int dx, dy;
   int ldx, ldy;
   int rdx, rdy;
   HPEN hOldPen;
   RECT rc;
   int radius = player.width / 2;

   if (minimapflags & MM_TEMPSAFE)
      hOldPen = (HPEN)SelectObject(hdc, hTempsafePen);
   else if (minimapflags & MM_NO_PVP)
      hOldPen = (HPEN)SelectObject(hdc, hNoPVPPen);
   else
      hOldPen = (HPEN)SelectObject(hdc, hPlayerPen);

   FindOffsets(player.width * 3 / 4, player.angle, &dx, &dy);
   FindOffsets(player.width / 4, player.angle+NUMDEGREES/4, &ldx, &ldy);
   FindOffsets(player.width / 4, player.angle-NUMDEGREES/4, &rdx, &rdy);

   if (config.showMapBlocking)
   {
      HBRUSH hOldBrush = (HBRUSH) SelectObject(hdc, hNullBrush);
      rc.left = x + (int)((player.x - radius) * scale);
      rc.top = y + (int)((player.y - radius) * scale);
      rc.right = x + (int)((player.x + radius) * scale);
      rc.bottom = y + (int)((player.y + radius) * scale);
      Ellipse(hdc, rc.left, rc.top, rc.right+1, rc.bottom+1);
      SelectObject(hdc, hOldBrush);
   }
   x += (int) (player.x * scale);
   y += (int) (player.y * scale);
   dx = (int) (dx * scale);
   dy = (int) (dy * scale);
   ldx = (int) (ldx * scale);
   ldy = (int) (ldy * scale);
   rdx = (int) (rdx * scale);
   rdy = (int) (rdy * scale);
   MoveToEx(hdc, x - dx, y - dy, NULL);
   LineTo(hdc, x + dx, y + dy);
   LineTo(hdc, x + ldx, y + ldy);
   LineTo(hdc, x + rdx, y + rdy);
   LineTo(hdc, x + dx, y + dy);

   // Indicate front of player
   //SetPixel(hdc, x + dx, y + dy, MAP_PLAYER_FRONT_COLOR);

   SelectObject(hdc, hOldPen);
}
/*****************************************************************************/
/*
 * MapDrawAnnotations:  Draw spots identifying map annotations.
 *   (x, y) is the upper-left corner of the drawing area on hdc.
 */
void MapDrawAnnotations( HDC hdc, MapAnnotation *annotations, int x, int y, float scaleToUse, Bool bMiniMap )
{
   int i, radius, new_x, new_y;

   MapMoveAnnotations( annotations, x, y, scaleToUse, bMiniMap );

   for (i=0; i < MAX_ANNOTATIONS; i++)
   {
      if (annotations[i].text[0] == 0)
         continue;

      new_x = x + (int) (annotations[i].x * scaleToUse);
      new_y =   y + (int) (annotations[i].y * scaleToUse);

      radius = max(MAP_ANNOTATION_MIN_SIZE / 2, (int) (MAP_ANNOTATION_SIZE * scaleToUse / 2));

      if (annotation.bits != NULL)
      {
         OffscreenWindowBackground(NULL, new_x - radius, new_y - radius, 
                               annotation.width, annotation.height);
         OffscreenStretchBlt(hdc, (int) (new_x - radius), (int) (new_y - radius), 
                          2 * radius, 2 * radius,
                          annotation.bits, 0, 0, 
                          annotation.width, annotation.height,
                          OBB_COPY | OBB_FLIP);
      }
   }
} 
/*****************************************************************************/
/*
 * MapEnterRoom:  Mark all walls as unseen when user enters a new room, and load
 *   the map from disk if necessary.
 */
void MapEnterRoom(room_type *room)
{
   int i;

   for (i = 0; i < room->num_walls; i++)
      room->walls[i].seen &= ~SR_SEEN;

   memset(room->annotations, 0, MAX_ANNOTATIONS * sizeof(MapAnnotation));
   room->annotations_changed = False;
   room->annotations_offset = 0;

   // Load map from file
   MapFileLoadRoom(room);

   fMapCacheValid = FALSE;
}
/*****************************************************************************/
/*
 * MapExitRoom:  Save map when leaving a room.
 */
void MapExitRoom(room_type *room)
{
   MapFileSaveRoom(room);
   if (pMapWalls)
      SafeFree(pMapWalls);
   pMapWalls = NULL;
}

/*****************************************************************************/
/*
 * MapZoom:  Change zoom factor on map.
 *   If direction > 0, zoom in.
 *   If direction < 0, zoom out.
 */
void MapZoom(int direction)
{
   DWORD now, dt;
   float increment;
   static DWORD last_time = 0;

//   if (!MapVisible())      commented by ajw
//     return;

   increment = (float) (MAP_ZOOM_INCREMENT * zoom);
   increment = (float) min(increment, MAP_ZOOM_INCREMENT * 8);
   increment = (float) max(increment, MAP_ZOOM_INCREMENT / 2);
   increment = (float) (SGN(direction) * increment);

   now = timeGetTime();
   dt = now - last_time;
   if (dt < MAP_ZOOM_DELAY && config.animate)
      increment = increment * dt / MAP_ZOOM_DELAY;
   last_time = now;

   zoom += increment;
   zoom = (float) max(zoom, MAP_ZOOM_MINZOOM);
   zoom = (float) min(zoom, MAP_ZOOM_MAXZOOM);

   fMapCacheValid = FALSE;

   //   Set flag that keeps background of the map fixed for the next draw.
   bDrawBackgroundStatic = True;

   RedrawAll();
}
/*****************************************************************************/
/*
 * MapScreenToRoom:  Given (x, y) in pixels measured from the origin of the graphics
 *   window, modify (x, y) to give the room coordinates on this location on the map.
 */
void MapScreenToRoom( int *x, int *y, Bool bMiniMap )
{
   if( !bMiniMap )
   {
      if( scale == 0.0 )
    return;
      *x = (int) ((*x - xoffset) / scale);
      *y = (int) ((*y - yoffset) / scale);
   }
   else
   {
      if( scaleMiniMap == 0.0 )
    return;
      *x = (int) ((*x - xoffsetMiniMap) / scaleMiniMap);
      *y = (int) ((*y - yoffsetMiniMap) / scaleMiniMap);
   }
}
/*****************************************************************************/
/*
 * MapShowAllWalls:  Set all walls in current room to seen (show = True)
 *   or hidden (show = False).
 */
void MapShowAllWalls(room_type *room, Bool show)
{
   WallData *wall;
   int i;
   Sidedef *sidedef;

   for (i = 0; i < room->num_walls; i++)
   {
      wall = &room->walls[i];

      sidedef = wall->pos_sidedef;
      if (sidedef == NULL)
         sidedef = wall->neg_sidedef;
      if (sidedef == NULL)
         continue;

      if (show && !(sidedef->flags & WF_MAP_NEVER))
         wall->seen |= SR_SEEN;

      if (!show && !(sidedef->flags & WF_MAP_ALWAYS))
         wall->seen &= ~SR_SEEN;
   }
}

void MapMiniSizeChanged(AREA *newArea)
{
   fMapCacheValid = FALSE;
}

static POINT cp;
static RECT rcMap,rcPage,rcMapBox;
static int xPrintMapScale;
static int yPrintMapScale;
static int mapPrintWidth,mapPrintHeight;
static int underspace = 0;
static HFONT hPrintTitle;
static HFONT hPrintLabel;
static HFONT hPrintText;
static int xMapBorder = 32;
static int yMapBorder = 32;
static int iMapLabelSpace = 8;

HFONT CreatePrinterFont(HDC hDC, HFONT font)
{
   HDC hScreenDC = CreateIC("DISPLAY",NULL,NULL,NULL);
   LOGFONT lf;

   GetObject(font, sizeof(LOGFONT), (void *) &lf);
   lf.lfHeight = MulDiv(lf.lfHeight,GetDeviceCaps(hDC, LOGPIXELSY),GetDeviceCaps(hScreenDC,LOGPIXELSY));
   return CreateFontIndirect(&lf);
}

static void PrintLine(HDC hdc, LPCSTR text)
{
   RECT clip;

   CopyRect(&clip,&rcPage);
   clip.top = cp.y;
   clip.left = cp.x;
   cp.y += DrawText(hdc,text,-1,&clip,DT_WORDBREAK);
}

static void ComputeMaxWallBoundaries(RECT *prc)
{
   int w;

   for (w = 0; w < current_room.num_walls; w++)
   {
      WallData *wall = &current_room.walls[w];
      if (w == 0)
      {
    prc->left = min(wall->x0,wall->x1);
    prc->right = max(wall->x0,wall->x1);
    prc->top = min(wall->y0,wall->y1);
    prc->bottom = max(wall->y0,wall->y1);
      }
      else
      {
    prc->left = min(prc->left,min(wall->x0,wall->x1));
    prc->right = max(prc->right,max(wall->x0,wall->x1));
    prc->top = min(prc->top,min(wall->y0,wall->y1));
    prc->bottom = max(prc->bottom,max(wall->y0,wall->y1));
      }
   }
   prc->left -= 10;
   prc->right += 10;
   prc->top -= 10;
   prc->bottom += 10;
}

static void DrawRect(HDC hdc, const RECT *prc, BOOL dotted)
{
   HPEN hPen,hOldPen;
   if (dotted)
      hPen = CreatePen(PS_DOT,1,RGB(0,0,0));
   else
      hPen = CreatePen(PS_SOLID,1,RGB(0,0,0));

   hOldPen = (HPEN) SelectObject(hdc,hPen);
   MoveToEx(hdc, prc->left, prc->top, NULL);
   LineTo(hdc, prc->right-1, prc->top);
   LineTo(hdc, prc->right-1, prc->bottom-1);
   LineTo(hdc, prc->left, prc->bottom-1);
   LineTo(hdc, prc->left, prc->top);
   SelectObject(hdc,hOldPen);
}

static void GetTextArea(HDC dc, LPCSTR text, SIZE *size)
{
   RECT rc;

   SetRect(&rc,0,0,100,100);
   size->cy = DrawText(dc,text,-1,&rc,DT_CALCRECT | DT_SINGLELINE);
   size->cx = rc.right;
}

static void GetPrintCoordinates(POINT *pt, int x, int y)
{
   pt->x = rcMapBox.left + xMapBorder +
      MulDiv(x - rcMap.left,mapPrintWidth,rcMap.right - rcMap.left);
   pt->y = rcMapBox.top + yMapBorder +
      MulDiv(y - rcMap.top,mapPrintHeight,rcMap.bottom - rcMap.top);
}

static void PrintMapAnnotations(HDC hdc)
{
   int i;
   POINT pt;
   RECT rcText;
   SIZE size;

   for (i=0; i < MAX_ANNOTATIONS; i++)
   {
      char buffer[] = "A";
      if (current_room.annotations[i].text[0] == 0)
    continue;
      GetPrintCoordinates(&pt,current_room.annotations[i].x,current_room.annotations[i].y);
      buffer[0] += i; // [A] .. [B] .. [C] .. ...
      SelectObject(hdc,hPrintLabel);
      GetTextArea(hdc,buffer,&size);
      TextOut(hdc,pt.x - size.cx/2, pt.y - size.cy/2,buffer,strlen(buffer));
      SetRect(&rcText,0,0,size.cx + iMapLabelSpace*2, size.cy + iMapLabelSpace);
      OffsetRect(&rcText,pt.x - size.cx/2 - iMapLabelSpace, pt.y - size.cy/2 - iMapLabelSpace);
      DrawRect(hdc,&rcText,TRUE);
   }
}

static void PrintAnnotations(HDC hdc)
{
   int i;

   for (i=0; i < MAX_ANNOTATIONS; i++)
   {
      RECT rcText;
      SIZE size;
      char buffer[] = "A";
      int xOld = cp.x;
      if (current_room.annotations[i].text[0] == 0)
    continue;
      buffer[0] += i; // [A] .. [B] .. [C] .. ...
      SelectObject(hdc,hPrintLabel);
      GetTextArea(hdc,buffer,&size);
      SetRect(&rcText,0,0,size.cx + iMapLabelSpace*2, size.cy + iMapLabelSpace);
      if (cp.y + size.cy > rcPage.bottom)
      {
    EndPage(hdc);
    cp.y = rcPage.top;
    RequestGamePing();
      }
      OffsetRect(&rcText,cp.x,cp.y);
      TextOut(hdc,cp.x+iMapLabelSpace, cp.y+iMapLabelSpace,buffer,strlen(buffer));
      DrawRect(hdc,&rcText,TRUE);
      cp.x += size.cx + iMapLabelSpace*2 + GetDeviceCaps(hdc, LOGPIXELSX) / 16;
      cp.y += iMapLabelSpace;
      SelectObject(hdc,hPrintText);
      PrintLine(hdc,current_room.annotations[i].text);
      cp.x = xOld;
      if (cp.y > rcText.bottom)
    cp.y += GetDeviceCaps(hdc, LOGPIXELSY) / 16;
      else
    cp.y = rcText.bottom + GetDeviceCaps(hdc, LOGPIXELSY) / 16;
   }
}

static void PrintMapWalls(HDC hdc)
{
   int w;

   for (w = 0; w < current_room.num_walls; w++)
   {
      POINT ptFrom, ptTo;
      WallData *wall = &current_room.walls[w];
      Sidedef *sidedef = wall->pos_sidedef;

      if (sidedef == NULL)
    sidedef = wall->neg_sidedef;
      if (sidedef == NULL)
    continue;

      if (((sidedef->flags & WF_MAP_ALWAYS) || (wall->seen & SR_SEEN))
         && !(sidedef->flags & WF_MAP_NEVER))
      {
         GetPrintCoordinates(&ptFrom, wall->x0, wall->y0);
         GetPrintCoordinates(&ptTo, wall->x1, wall->y1);
    MoveToEx(hdc,ptFrom.x,ptFrom.y,NULL);
    LineTo(hdc,ptTo.x,ptTo.y);
      }
   }
}

void PrintMap(BOOL useDefault)
{
   PRINTDLG pageSetup;
   
   memset(&pageSetup,0,sizeof(pageSetup));
   memset(&cp,0,sizeof(cp)); // set initial cursor point to 0,0

   pageSetup.lStructSize = sizeof(pageSetup);
   pageSetup.hwndOwner = hMain;
   pageSetup.Flags = PD_RETURNDC|PD_NOPAGENUMS|PD_ALLPAGES|PD_HIDEPRINTTOFILE|PD_NOSELECTION;
   if (useDefault)
   {
      pageSetup.Flags |= PSD_RETURNDEFAULT;
   }
   RequestGamePing();
   if (PrintDlg(&pageSetup))
   {
      char buffer[256];
      DOCINFO doc;
      HFONT hOldFont;
      TEXTMETRIC tm;

      RequestGamePing();
      SetMapMode(pageSetup.hDC,MM_TEXT);
      SetBkMode(pageSetup.hDC,TRANSPARENT);
      hPrintTitle = CreatePrinterFont(pageSetup.hDC,GetFont(FONT_MAP_TITLE));
      hPrintLabel = CreatePrinterFont(pageSetup.hDC,GetFont(FONT_MAP_LABEL));
      hPrintText = CreatePrinterFont(pageSetup.hDC,GetFont(FONT_MAP_TEXT));
      yMapBorder = GetDeviceCaps(pageSetup.hDC, LOGPIXELSY) / 4;
      xMapBorder = GetDeviceCaps(pageSetup.hDC, LOGPIXELSX) / 4;

      hOldFont = (HFONT) SelectObject(pageSetup.hDC,hPrintLabel);
      GetTextMetrics(pageSetup.hDC,&tm);
      iMapLabelSpace = tm.tmDescent;

      GetWindowText(hMain,buffer,sizeof(buffer));
      memset(&doc,0,sizeof(doc));
      doc.cbSize = sizeof(doc);
      doc.lpszDocName = buffer;

      StartDoc(pageSetup.hDC,&doc);
      wsprintf(buffer,"Map of %s",LookupNameRsc(player.room_name_res));

      GetClipBox(pageSetup.hDC,&rcPage);
      rcPage.top += GetDeviceCaps(pageSetup.hDC, LOGPIXELSY) / 2;
      rcPage.left += GetDeviceCaps(pageSetup.hDC, LOGPIXELSX);
      rcPage.right -= GetDeviceCaps(pageSetup.hDC, LOGPIXELSX);
      rcPage.bottom -= GetDeviceCaps(pageSetup.hDC, LOGPIXELSY);
      cp.y = rcPage.top;
      cp.x = rcPage.left;
      SelectObject(pageSetup.hDC,hPrintTitle);
      PrintLine(pageSetup.hDC,buffer);
      cp.y += yMapBorder;
      ComputeMaxWallBoundaries(&rcMap);
      if ((rcMap.right-rcMap.left) > (rcMap.bottom-rcMap.top))
      { // wider than it is tall
    mapPrintWidth = rcPage.right - rcPage.left - xMapBorder*2;
    mapPrintHeight = MulDiv(rcMap.bottom-rcMap.top,mapPrintWidth,rcMap.right-rcMap.left);
      }
      else
      { // taller than it is wide
    mapPrintHeight = ((rcPage.bottom - rcPage.top) / 2) - yMapBorder*2;
    mapPrintWidth = MulDiv(rcMap.right-rcMap.left,mapPrintHeight,rcMap.bottom-rcMap.top);
      }
      SetRect(&rcMapBox,0,0,mapPrintWidth + xMapBorder*2, mapPrintHeight + yMapBorder * 2);
      OffsetRect(&rcMapBox,rcPage.left,cp.y+10);
      DrawRect(pageSetup.hDC,&rcMapBox,FALSE);
      RequestGamePing();
      PrintMapWalls(pageSetup.hDC);
      PrintMapAnnotations(pageSetup.hDC);
      cp.y = rcMapBox.bottom + yMapBorder;
      PrintAnnotations(pageSetup.hDC);

      SelectObject(pageSetup.hDC,hOldFont);
      DeleteObject(hPrintTitle);
      DeleteObject(hPrintLabel);
      DeleteObject(hPrintText);
      EndDoc(pageSetup.hDC);
      RequestGamePing();
   }
}
