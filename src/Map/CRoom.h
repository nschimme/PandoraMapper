/*
 *  Pandora MUME mapper
 *
 *  Copyright (C) 2000-2009  Azazello
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

//
// C++ Interface: croom
//
// Description: 
//
//
// Author: Azazello <aza@alpha>, (C) 2005
//
// Copyright: See COPYING file that comes with this distribution
//
//
#ifndef CROOM_H
#define CROOM_H

#include <QByteArray>

#include "defines.h"

#include "Map/CRegion.h"
#include "Renderer/CSquare.h"

#include "map.pb.h"


struct room_flag_data {
  QByteArray name;
  QByteArray xml_name;
  unsigned int flag;
};

extern const struct room_flag_data room_flags[];

class Strings_Comparator {
    private:
        static const int MAX_N = MAX_LINES_DESC * 80;
        static const int MAX_M = MAX_LINES_DESC * 80;
        int D[ MAX_N ] [MAX_M ];
    public:
        int compare(QByteArray pattern, QByteArray text);
        int compare_with_quote(QByteArray str, QByteArray text, int quote);
        int strcmp_roomname(QByteArray name, QByteArray text);
        int strcmp_desc(QByteArray name, QByteArray text);

};

extern Strings_Comparator comparator;

class CRoomManager;

class CRoom {
    // inner serializable (protocol buffers) object
    mapdata::Room room;

    // old member and fields
//    enum ExitFlags { EXIT_NONE = 0, EXIT_UNDEFINED, EXIT_DEATH};

//    unsigned int    flags;
//    QByteArray      name; 			//
//    QByteArray      note; 			// note, if needed, additional info etc
//    QByteArray      noteColor;      // note color in this room
//    QByteArray      desc;			// descrition
//    char            sector;                 /* terrain marker */
//    CRegion         *region;               /* region of this room */
    
//    QByteArray    doors[6];		/* if the door is secret */
//    unsigned  char exitFlags[6];
//    unsigned int    id; 		        /* identifier, public for speed up - its very often used  */
//    int x, y, z;		/* coordinates on our map */


    CSquare			*square;  		/* which square this room belongs to */
    CRegion         *region;

    CRoomManager    *parent;
public:
    CRoom(CRoomManager *parent);
    ~CRoom();
    
    RoomId getId() { return room.id(); }
    void setId(RoomId id) { room.set_id(id); }

    CRoom* getExitRoom(ExitDirection dir);

    RoomId getExitLeadsTo(ExitDirection dir);

    void CRoom::setSector(RoomTerrainType val);


    QByteArray getName();
    QByteArray getDesc();
    RoomTerrainType getTerrain();
    QByteArray getNote();
    
    QByteArray getNoteColor();
    void setNoteColor(QByteArray color);
    
    void setDesc(QByteArray newdesc);
    void setName(QByteArray newname);
    void setTerrain(char terrain);
    void setNote(QByteArray note);

    void setSquare(CSquare *square);
    CSquare* getSquare() { return square; }
    
    bool isDescSet();
    bool isNameSet();
    bool isEqualNameAndDesc(CRoom *room);
    
    QString toolTip();
    
    void setModified(bool b);
    bool isConnected(ExitDirection dir);
    void sendRoom();
    
    // door stuff
    int setDoor(ExitDirection dir, QByteArray door);
    void removeDoor(ExitDirection dir);
    QByteArray getDoor(ExitDirection dir);
    bool isDoorSet(ExitDirection dir);
    bool isDoorSecret(ExitDirection dir);
    
    void disconnectExit(ExitDirection dir);       /* just detaches the connection */
    void removeExit(ExitDirection dir);           /* also removes the door */
    void setExit(ExitDirection dir, CRoom *room);
    void setExit(ExitDirection dir, RoomId id);
//    CRoom *getExit(int dir);
    bool isExitLeadingTo(ExitDirection dir, CRoom *room);
    
    bool isExitDeath(ExitDirection dir);
    void setExitDeath(ExitDirection dir);

    bool isExitNormal(ExitDirection dir);
    bool isExitPresent(ExitDirection dir);       /* if there is anything at all in this direction, deathtrap, undefined exit or normal one */
    bool isExitUndefined(ExitDirection dir);
    void setExitUndefined(ExitDirection dir);
    
    
    bool anyUndefinedExits();
    
    
    // coordinates     
    void setX(int x);
    void setY(int x);
    void setZ(int x);
    void simpleSetZ(int val); // does not perform any Plane operations (see rendering) on  CPlane in Map.h
    
    int getX();
    int getY();
    int getZ();
    
    int descCmp(QByteArray desc);
    int roomnameCmp(QByteArray name);
    
    
    QByteArray getRegionName();
    CRegion *getRegion();
    void setRegion(QByteArray name);
    void setRegion(CRegion *reg);
    QByteArray getSecretsInfo();
    QByteArray getDoorAlias(int i);
    
    char dirbynum(ExitDirection dir);

    void rebuildDisplayList() {if (square) square->rebuildDisplayList(); }
};

#endif