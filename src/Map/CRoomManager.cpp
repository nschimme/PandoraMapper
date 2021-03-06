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

/* Implementation of room manager of Pandora Project (c) Azazello 2003 */
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <QDateTime>
#include <vector>
#include <iostream>
#include <fstream>
#include <QProgressDialog>
//using namespace std;

#include "defines.h"
#include "CConfigurator.h"
#include "utils.h"

#include "Map/CRoomManager.h"
#include "Map/CTree.h"

#include "Engine/CStacksManager.h"
#include "Proxy/CDispatcher.h"
#include "Gui/mainwindow.h"


#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/coded_stream.h>

using namespace google::protobuf::io;

//class CRoomManager __Map;

/*------------- Constructor of the room manager ---------------*/
CRoomManager::CRoomManager()
{
    init();
    blocked = false;
}


CRoomManager::~CRoomManager()
{
    rooms.clear();
    regions.clear();
    delete roomNamesTree;
}


void CRoomManager::init()
{
//	QWriteLocker locker(&mapLock);

    print_debug(DEBUG_ROOMS,"Roommanager INIT.\r\n");

    next_free = 1;

    print_debug(DEBUG_ROOMS, "In roomer.init()");

    /* adding first (empty) root elements to the lists */
    rooms.clear();
    regions.clear();



    CRegion *region = new CRegion(this);
    region->setName("default");

    regions.push_back(region);

    roomNamesTree = new CTree();

    ids[0] = NULL;
    planes = NULL;
}

TTree* CRoomManager::findByName(QByteArray last_name) {
    return roomNamesTree->findByName(last_name);
}


CRoom* CRoomManager::createRoom(RoomId id, int x, int y, int z)
{
    CRoom *addedroom = new CRoom(this);

    addedroom->setId( id );

    addedroom->setX( x );
    addedroom->setY( y );
    addedroom->simpleSetZ( z );

    return addedroom;
}


CRoom* CRoomManager::createRoom(QByteArray name, QByteArray desc, int x, int y, int z)
{
    fixFreeRooms();	// making this call just for more safety - might remove

    CRoom *addedroom = new CRoom(this);

    addedroom->setId( next_free );
    addedroom->setName(name);
    addedroom->setDesc(desc);

    addedroom->setX(x);
    addedroom->setY(y);
    addedroom->simpleSetZ(z);

    addRoom(addedroom);

    return addedroom;
}



void CRoomManager::rebuildRegion(CRegion *reg)
{
	if (reg == NULL)
		return;

    QVector<CRoom *> rooms = getRooms();
    for (unsigned int i = 0; i < size(); i++) {
        CRoom *r = rooms[i];
        if (r->getRegion() == reg)
        	// this only sets a flag, so it should not be a problem to "rebuild" squares list multiple times
        	r->rebuildDisplayList();
    }

}

void CRoomManager::clearAllSecrets()
{
    bool mark[MAX_ROOMS];
    CRoom *r;
    unsigned int i;
    unsigned int z;

    // "wave" over all rooms reacheable over non-secret exits.
    memset(mark, 0, MAX_ROOMS);

    CStacksManager stacker;
    stacker.clear();
    stacker.put(1);
    stacker.swap();

    MapBlocker blocker(*this);

    QProgressDialog progress("Removing secret exits...", "Abort", 0, MAX_ROOMS, renderer_window);
    progress.setWindowModality(Qt::ApplicationModal);
    progress.show();


    while (stacker.amount() != 0) {
        for (i = 0; i < stacker.amount(); i++) {
            progress.setValue(i);
            r = stacker.get(i);
            mark[r->getId()] = true;
            for (z = 0; z <= 5; z++) {
                ExitDirection zDir = static_cast<ExitDirection>(z);
                if (r->isConnected(zDir) && mark[ r->getExitLeadsTo(zDir) ] != true  && r->isDoorSecret(zDir) != true  )
                    stacker.put(r->getExitLeadsTo(zDir));
            }
        }
        stacker.swap();
    }

    progress.setValue(0);
    // delete all unreached rooms
    for (i = 0; i < MAX_ROOMS; i++) {
        progress.setValue(i);
        r = getRoom( i );
        if (r == NULL)
            continue;
        if (r) {
            if (mark[r->getId()] == false) {
                deleteRoom(r, 0);
                continue;
            }
        }

    }

    QVector<CRoom *> rooms = getRooms();
    // roll over all still accessible rooms and delete the secret doors if they are still left in the database
    for (i = 0; i < size(); i++) {
        r = rooms[i];
        if (r) {
            for (z = 0; z <= 5; z++) {
                ExitDirection zDir = static_cast<ExitDirection>(z);
                if ( r->isDoorSecret(zDir) == true ) {
                    print_debug(DEBUG_ROOMS,"Secret door was still in database...\r\n");
                    r->removeDoor(zDir);
                }
            }
        }

    }

}


CRoom * CRoomManager::isDuplicate(CRoom *addedroom)
{
    CRoom *r;
    unsigned int i;
    ExitDirection j;

    print_debug(DEBUG_ANALYZER, "Room-desc check for new room");

    j = ED_UNKNOWN;

    /* theory - new added room has only one exit dir defined - the one we came from */
    /* so if we find same looking (name, desc) room in base with the same undefined */
    /* exit as the defined exit in current room, we can merge them. */
    if (addedroom->getName().isEmpty()) {
        /* now thats sounds bad ... */
        print_debug(DEBUG_ANALYZER, "ERROR: in check_description() - empty roomname in new room.\r\n");
        return NULL;
    }

    if (addedroom->getDesc().isEmpty()) {
        send_to_user("--[Pandora: Error, empty roomdesc in new added room.\r\n");
        addedroom->setDesc("");
        return addedroom;
    }

    if (!conf->getAutomerge())
        return addedroom;

    /* find the only defined exit in new room - the one we came from */
    for (i = 0; i <= 5; i++) {
        ExitDirection iDir = static_cast<ExitDirection>(i);
        if ( addedroom->isConnected(iDir) ) {
            j = iDir;
            break;
        }
    }

    for (i = 0; i < size(); i++) {
        r = rooms[i];
        if (addedroom->getId() == r->getId() || r->getDesc() == "" || r->getName() == "") {
          continue;
        }

        /* in this case we do an exact match for both roomname and description */
        if (addedroom->getName() == r->getName() && addedroom->getDesc() == r->getDesc()) {
            CRoom *result = tryMergeRooms(r, addedroom, j);
            if (result != NULL)
                return result;

        }
    }

    print_debug(DEBUG_ANALYZER, "------- Returning with return 0\r\n");
    return addedroom;
}


// for mmerge command
CRoom* CRoomManager::findDuplicateRoom(CRoom *orig)
{
	CRoom* t;

//	QWriteLocker locker(&mapLock);
    for (unsigned int i = 0; i < size(); i++) {
        t = rooms[i];
        if (orig->getId() == t->getId() || t->isDescSet() == false || t->isNameSet() == false ) {
          continue;
        }

        /* in this case we do an exact match for both roomname and description */
        if (orig->isEqualNameAndDesc(t) == true)  {
            return t;
        }
    }

    return NULL;
}


//------------ merge_rooms -------------------------
CRoom* CRoomManager::tryMergeRooms(CRoom *r, CRoom *copy, ExitDirection j)
{
  CRoom *p;

  print_debug(DEBUG_ROOMS, "entering tryMergeRooms...");
//  QWriteLocker locker(&mapLock);

  if (j == ED_UNKNOWN) {
    /* oneway ?! */
    print_debug(DEBUG_ROOMS, "fixing one way in previous room, repointing at merged room");

     p = getRoom(oneway_room_id);
     for (unsigned int i = 0; i <= 5; i++) {
         ExitDirection iDir = static_cast<ExitDirection>(i);
         if (p->isExitLeadingTo(iDir, copy) == true)
             p->setExitLeadsTo(iDir, r);
     }

    smallDeleteRoom(copy);

    return r;
  }

  if ( r->isExitUndefined(j) ) {
    r->setExitLeadsTo(j, copy->getExitLeadsTo(j) );

    p = copy->getExitRoom(j);
    if (p->isExitLeadingTo( reversenum(j), copy) == true)
        p->setExitLeadsTo( reversenum(j), r);

    smallDeleteRoom(copy);

    return r;
  }
  return NULL;
}

/* ------------ fixfree ------------- */
void CRoomManager::fixFreeRooms()
{
    RoomId i;

    for (i = 1; i < MAX_ROOMS; i++)
	if (ids[i] == NULL) {
	    next_free = i;
	    return;
	}

    print_debug(DEBUG_ROOMS, "roomer: error - no more space for rooms in ids[] array! reached limit\n");
    exit(1);
}

void CRoomManager::addRoom(CRoom *room)
{
    if (ids[room->getId()] != NULL) {
        print_debug(DEBUG_ROOMS, "Error while adding new element to database! This id already exists!\n");
    	// Whaaaat?
        //exit(1);
        return;
    }

    rooms.push_back(room);
    ids[room->getId()] = room;	/* add to the first array */
    roomNamesTree->addName(room->getName(), room->getId());	/* update name-searhing engine */

    fixFreeRooms();
    addToPlane(room);
}
/* ------------ addroom ENDS ---------- */




/*------------- Constructor of the room manager ENDS  ---------------*/

CRegion *CRoomManager::getRegionByName(QByteArray name)
{
	// TODO: threadsafety the class regions QMutexLocker locker(mapLock);

	CRegion    *region;
    for (int i=0; i < regions.size(); i++) {
        region = regions[i];
        if (region->getName() == name)
            return region;
    }
    return NULL;
}

bool CRoomManager::addRegion(QByteArray name)
{
    CRegion    *region;
	// TODO: threadsafety the class regions QMutexLocker locker(mapLock);

    if (getRegionByName(name) == false) {
        region = new CRegion(this);
        region->setName( name );
        regions.push_back(region);
        return true;
    } else {
        return false;
    }

}

void CRoomManager::addRegion(CRegion *reg)
{
	// TODO: threadsafety the class regions QMutexLocker locker(mapLock);

	if (reg != NULL)
        regions.push_back(reg);
}




void CRoomManager::sendRegionsList()
{
    CRegion    *region;
	// TODO: threadsafety the class regions QMutexLocker locker(mapLock);


    send_to_user( "Present regions: \r\n");
    for (int i=0; i < regions.size(); i++) {
        region = regions[i];
        send_to_user("  %s\r\n", (const char *) region->getName() );
    }


}

QList<CRegion *> CRoomManager::getAllRegions()
{
	// TODO: threadsafety the class regions QMutexLocker locker(mapLock);
	return regions;
}


/* -------------- reinit ---------------*/
void CRoomManager::reinit()
{
//	unlock();
//	QWriteLocker locker(&mapLock);

	next_free = 1;
    {
        CPlane *p, *next;

        print_debug(DEBUG_ROOMS,"Resetting Cplane structures ... \r\n");
        p = planes;
        while (p) {
            next = p->next;
            delete p;
            p = next;
        }
        planes = NULL;
    }

    memset(ids, 0, MAX_ROOMS * sizeof (CRoom *) );
    rooms.clear();
    roomNamesTree->reinit();
}

/* -------------- reinit ENDS --------- */

/* ------------ delete_room --------- */
/* mode 0 - remove all links in other rooms together with exits and doors */
/* mode 1 - keeps the doors and exits in other rooms, but mark them as undefined */
void CRoomManager::deleteRoom(CRoom *r, int mode)
{
    int k;
    int i;

    if (r->getId() == 1) {
    	print_debug(DEBUG_ROOMS,"Cant delete base room!\n");
    	return;
    }

    /* have to do this because of possible oneways leading in */
    for (i = 0; i < rooms.size(); i++)
        for (k = 0; k <= 5; k++) {
            ExitDirection kDir = static_cast<ExitDirection>(k);
            if (rooms[i]->isExitLeadingTo(kDir, r) == true) {
                if (mode == 0) {
                    rooms[i]->removeExit(kDir);
                } else if (mode == 1) {
                    rooms[i]->setExitUndefined(kDir);
                }
	    	}
        }
    smallDeleteRoom(r);
}

/* --------- _delete_room ENDS --------- */

/* ------------ small_delete_room --------- */
void CRoomManager::smallDeleteRoom(CRoom *r)
{
    if (r->getId() == 1) {
		print_debug(DEBUG_ROOMS,"ERROR (!!): Attempted to delete the base room!\n");
		return;
    }

	removeFromPlane(r);
    //FIXME: add notificiation signal
    //stacker.removeRoom(r->getId());
    selections.unselect(r->getId());
	if (engine->addedroom == r)
        engine->resetAddedRoomVar();

    renderer_window->renderer->deletedRoom = r->getId();

    int i;
    ids[ r->getId() ] = NULL;

    for (i = 0; i < rooms.size(); i++)
        if (rooms[i]->getId() == r->getId() ) {
            print_debug(DEBUG_ROOMS,"Deleting the room from rooms vector.\r\n");
            rooms.remove(i);
            break;
        }

    delete r;

    fixFreeRooms();
    toggle_renderer_reaction();
}
/* --------- small_delete_room ENDS --------- */

// this function is only called as result of CRoom.setZ and
// addToPlace() functions.
// addToPlane is protected by CRoomManager locks
// CRoom.setZ() stays as open issue aswell as the whole
// set of writing CRoom functions ...
void CRoomManager::removeFromPlane(CRoom *room)
{
    CPlane *p;

    if (planes == NULL)
    	return;

    p = planes;
    while (p->z != room->getZ()) {
        if (!p) {
            print_debug(DEBUG_ROOMS," FATAL ERROR. remove_fromplane() the given has impossible Z coordinate!\r\n");
            return;     /* no idea what happens next ... */
        }
        p = p->next;
    }

    p->squares->remove(room);
}

void CRoomManager::expandPlane(CPlane *plane, CRoom *room)
{
    CSquare *p, *new_root = NULL;
    int size;

    p = plane->squares;

    while ( p->isInside(room) != true ) {
        /* plane fork/expanding cycle */

        size = p->rightx - p->leftx;

        switch ( p->getMode(room) )
        {
            case  CSquare::Left_Upper:
                new_root = new CSquare(p->leftx - size, p->lefty + size, p->rightx, p->righty);
                new_root->subsquares[ CSquare::Right_Lower ] = p;
                break;
            case  CSquare::Right_Upper:
                new_root = new CSquare(p->leftx,  p->lefty + size, p->rightx + size, p->righty);
                new_root->subsquares[ CSquare::Left_Lower ] = p;
                break;
            case  CSquare::Right_Lower:
                new_root = new CSquare(p->leftx,  p->lefty, p->rightx + size, p->righty - size);
                new_root->subsquares[ CSquare::Left_Upper ] = p;
                break;
            case  CSquare::Left_Lower:
                new_root = new CSquare(p->leftx - size,  p->lefty, p->rightx , p->righty - size);
                new_root->subsquares[ CSquare::Right_Upper ] = p;
                break;
        }

        p = new_root;
    }

/*    printf("Ok, it fits. Adding!\r\n");
*/
    p->add(room);
    plane->squares = p;
}


void  CRoomManager::addToPlane(CRoom *room)
{
    CPlane *p, *prev, *tmp;

    // is protected by CRoomManager locker
    //	QMutexLocker locker(mapLock);

    if (planes == NULL) {
        planes = new CPlane(room);
        return;
    }

    p = planes;
    prev = NULL;
    while (p) {
        if (room->getZ() < p->z) {
            tmp = new CPlane(room);
            tmp->next = p;
            if (prev)
                prev->next = tmp;
            else
                planes = tmp;
            return;
        }
        /* existing plane with already set borders */
        if (room->getZ() == p->z) {
            expandPlane(p, room);
            return;
        }
        prev = p;
        p = p->next;
    }

    /* else .. this is a plane with highest yet found Z coordinate */
    /* we add it to the end of the list */
    prev->next = new CPlane(room);
}

QList<int> CRoomManager::searchNames(QString s, Qt::CaseSensitivity cs)
{
    QList<int> results;

//	QReadLocker locker(&mapLock);

    for (int i = 0; i < rooms.size(); i++) {
        if (QString(rooms[i]->getName()).contains(s, cs)) {
            results << rooms[i]->getId();
        }
    }

    return results;
}

QList<int> CRoomManager::searchDescs(QString s, Qt::CaseSensitivity cs)
{
    QList<int> results;

//    QReadLocker locker(&mapLock);

    for (int i = 0; i < rooms.size(); i++) {
        if (QString(rooms[i]->getDesc()).contains(s, cs)) {
            results << rooms[i]->getId();
        }
    }

    return results;
}

QList<int> CRoomManager::searchNotes(QString s, Qt::CaseSensitivity cs)
{
    QList<int> results;

//    QReadLocker locker(&mapLock);

	for (int i = 0; i < rooms.size(); i++) {
        if (QString(rooms[i]->getNote()).contains(s, cs)) {
            results << rooms[i]->getId();
        }
    }

    return results;
}

QList<int> CRoomManager::searchExits(QString s, Qt::CaseSensitivity cs)
{
    QList<int> results;

//    QReadLocker locker(&mapLock);

    for (int i = 0; i < rooms.size(); i++) {
        for (int j = 0; j <= 5; j++) {
            ExitDirection jDir = static_cast<ExitDirection>(j);
            if (rooms[i]->isDoorSecret( jDir ) == true) {
                if (QString(rooms[i]->getDoor(jDir)).contains(s, cs)) {
                    results << rooms[i]->getId();
                }
            }
        }
    }

    return results;
}



bool CRoomManager::loadMap(QString filename)
{
    MapBlocker blocker(*this);

    reinit();


    std::ifstream mFs(filename.toLocal8Bit(), std::ios::in | std::ios::binary);

    if (!mFs.good()) {
        throw std::runtime_error(std::string("Failed to open map file"));
    }

    IstreamInputStream * _IstreamInputStream = new IstreamInputStream(&mFs);
    CodedInputStream * _CodedInputStream = new CodedInputStream(_IstreamInputStream);

    // check magic
    {
        unsigned __int32 magic;

        bool ret = _CodedInputStream->ReadVarint32(&magic);
        if (!ret || magic != pmf_magic_number) {
            throw std::runtime_error(std::string("The file given does not look like PMF map file"));
        }
    }

    // read the header
    mapdata::MapHeader header;
    {
        unsigned __int32 size;

        bool ret;
        if ( ret = _CodedInputStream->ReadVarint32(&size) )
        {
            CodedInputStream::Limit msgLimit = _CodedInputStream->PushLimit(size);
            if ( ret = header.ParseFromCodedStream(_CodedInputStream) )
            {
                _CodedInputStream->PopLimit(msgLimit);
            }
        }
        if (!ret) {
            throw std::runtime_error("Failed to parse the header");
        }

    }

    // load areas (regions) data
    for (int i = 0; i < header.areas_amount(); i++) {
        mapdata::Area area;
        unsigned __int32 size;

        bool ret;
        if ( ret = _CodedInputStream->ReadVarint32(&size) )
        {
            CodedInputStream::Limit msgLimit = _CodedInputStream->PushLimit(size);
            if ( ret = area.ParseFromCodedStream(_CodedInputStream) )
            {
                _CodedInputStream->PopLimit(msgLimit);

                // process area data
                CRegion *region = new CRegion(this);
                region->setName(area.name().c_str());
                for (int k = 0; k < area.alias_size(); k++) {
                    auto alias = area.alias(k);
                    region->addDoor(alias.name().c_str(), alias.door().c_str());
                }
                addRegion(region);
            }
        }
        if (!ret) {
            throw std::runtime_error("Failed to parse area data");
        }

    }


    // load rooms data
    for (int i = 0; i < header.rooms_amount(); i++) {
        CRoom *r = new CRoom(this);
        mapdata::Room *roomData = r->getInnerRoomData();
        unsigned __int32 size;

        bool ret;
        if ( ret = _CodedInputStream->ReadVarint32(&size) )
        {
            CodedInputStream::Limit msgLimit = _CodedInputStream->PushLimit(size);
            if ( ret = roomData->ParseFromCodedStream(_CodedInputStream) )
            {
                _CodedInputStream->PopLimit(msgLimit);
                addRoom(r);
            }
        }

        if (!ret) {
            throw std::runtime_error("Failed to parse the room entry");
        }

    }

    return true;
}

bool CRoomManager::saveMap(QString filename)
{
    MapBlocker blocker(*this);

    std::ofstream mFs(filename.toLocal8Bit(), std::ios::out | std::ios::binary);

    if (!mFs.good()) {
        setBlocked(false);
        throw std::runtime_error(std::string("Failed to open map file"));
    }

    OstreamOutputStream * _OstreamOutputStream = new OstreamOutputStream(&mFs);
    CodedOutputStream * _CodedOutputStream = new CodedOutputStream(_OstreamOutputStream);

    // write the magic number
    _CodedOutputStream->WriteVarint32(pmf_magic_number);

    // construct header
    mapdata::MapHeader header;
    header.set_areas_amount(regions.size());
    header.set_rooms_amount( size() );

    // write the header
    _CodedOutputStream->WriteVarint32(header.ByteSize());
    if ( !header.SerializeToCodedStream(_CodedOutputStream) ) {
        throw std::runtime_error(std::string("Failed to serialize the header of the map"));
    }


    // save regions data
    {
        QList<CRegion *> regions = getAllRegions();
        for (int i=0; i < regions.size(); i++) {
            mapdata::Area area;
            CRegion    *region = regions[i];

            area.set_name(region->getName().constData());

            QMap<QByteArray, QByteArray> doors = region->getAllDoors();
            QMapIterator<QByteArray, QByteArray> iter(doors);
            while (iter.hasNext()) {
                iter.next();

                mapdata::Area::Alias* alias = area.add_alias();
                alias->set_name(iter.key());
                alias->set_door(iter.value());
            }

            // write the region
            _CodedOutputStream->WriteVarint32(area.ByteSize());
            if ( !area.SerializeToCodedStream(_CodedOutputStream) ) {
                throw std::runtime_error(std::string("Failed to serialize region data"));
            }
        }
    }


    // save rooms data
    for (int i = 0; i < rooms.size(); i++){
        CRoom *r = rooms[i];

        mapdata::Room* roomData = r->getInnerRoomData();

        _CodedOutputStream->WriteVarint32(roomData->ByteSize());
        if ( !roomData->SerializeToCodedStream(_CodedOutputStream) ) {
            throw std::runtime_error(std::string("Failed to serialize the header of the map"));
        }
    }


    delete _CodedOutputStream;
    delete _OstreamOutputStream;
    return true;
}
