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

#ifndef ROOMSMANAGER_H
#define ROOMSMANAGER_H

#include <QVector>
#include <QObject>
#include <QThread>
#include <QReadWriteLock>
#include <QReadLocker>
#include <QWriteLocker>


#include "defines.h"

#include "Map/CRoom.h"
#include "Map/CRegion.h"
#include "Gui/CSelectionManager.h"


class CPlane;
class CSquare;
class CTree;
struct TTree;



class CRoomManager : public QObject {
	Q_OBJECT

    static const unsigned __int32       pmf_magic_number = 0x00504D46; // " PMF"

	QList<CRegion *>    regions;
    QVector<CRoom* > 	rooms;   		/* rooms */
    CRoom* 				ids[MAX_ROOMS];	/* array of pointers */

    CPlane        		*planes;        /* planes/levels of the map, sorted by the Z coordinate, lower at first */

    CTree               *roomNamesTree;


//    void deleteRoomUnlocked(CRoom* r, int mode);  /* user interface function */
//    void smallDeleteRoomUnlocked(CRoom* r);  /* user interface function */

    bool	blocked;
public:
    CRoomManager();
    virtual ~CRoomManager();
    void init();
    void reinit();			/* reinitializer/utilizer */

    QVector<CRoom* > getRooms() { return rooms; }
    CPlane* getPlanes() { return planes; }

    RoomId next_free; 	/* next free id */
    RoomId oneway_room_id;

    unsigned int size()  { return rooms.size(); }


    CSelectionManager   selections;

    /* plane support */
    void          addToPlane(CRoom* room);
    void          removeFromPlane(CRoom* room);
    void          expandPlane(CPlane *plane, CRoom* room);

    CTree*        getRoomNamesTree() { return roomNamesTree; }


    CRoom* createRoom(QByteArray name, QByteArray desc, int x, int y, int z);
    CRoom* createRoom(RoomId id, int x = 0, int y = 0, int z = 0);

    void addRoom(CRoom* room);
    inline CRoom* getRoom(RoomId id)        {
		if (id < MAX_ROOMS)
			return ids[id];
		else
			return NULL;
    }

    inline QByteArray getName(RoomId id)  {
    	if (ids[id]) return (*(ids[id])).getName(); return "";
    }

    CRoom *tryMergeRooms(CRoom* room, CRoom* copy, ExitDirection j);
    CRoom *isDuplicate(CRoom* addedroom);

    void fixFreeRooms();
    CRegion* getRegionByName(QByteArray name);
    bool addRegion(QByteArray name);
    void addRegion(CRegion *reg);
    void rebuildRegion(CRegion *reg);

    void sendRegionsList();

    QList<CRegion *> getAllRegions();

    void deleteRoom(CRoom* r, int mode);
    void smallDeleteRoom(CRoom* r);

    TTree* findByName(QByteArray last_name);

    QList<int> searchNames(QString s, Qt::CaseSensitivity cs);
    QList<int> searchDescs(QString s, Qt::CaseSensitivity cs);
    QList<int> searchNotes(QString s, Qt::CaseSensitivity cs);
    QList<int> searchExits(QString s, Qt::CaseSensitivity cs);


    CRoom* findDuplicateRoom(CRoom *orig);

    bool loadMap(QString filename);
    bool saveMap(QString filename);




    void loadXmlMap(QString filename);
    void saveXmlMap(QString filename);
    void clearAllSecrets();

    bool loadMMapperMap(QString filename);


    void setBlocked(bool b) { blocked = b; }
    bool isBlocked() { return blocked; }
};

class MapBlocker {
  public:
    MapBlocker(CRoomManager &in_data) : data(in_data) { data.setBlocked(true);}
    ~MapBlocker() {data.setBlocked(false);}
  private:
    CRoomManager & data;
};


//extern class CRoomManager __Map;/* room manager */

#endif

