#ifndef _GAME_ESM_RECLISTS_H
#define _GAME_ESM_RECLISTS_H

#include "components/esm/records.hpp"
#include <map>
#include <string>
#include <algorithm>
#include <cctype>
#include <assert.h>
#include <stdexcept>
#include <iterator>

namespace ESMS
{
  using namespace ESM;

  struct RecList
  {
    virtual void load(ESMReader &esm, const std::string &id) = 0;
    virtual int getSize() = 0;

    static std::string toLower (const std::string& name)
    {
        std::string lowerCase;

        std::transform (name.begin(), name.end(), std::back_inserter (lowerCase),
            (int(*)(int)) std::tolower);

        return lowerCase;
    }
  };

  typedef std::map<int,RecList*> RecListList;

  template <typename X>
  struct RecListT : RecList
  {
    typedef std::map<std::string,X> MapType;

    MapType list;

    // Load one object of this type
    void load(ESMReader &esm, const std::string &id)
    {
      std::string id2 = toLower (id);
      list[id2].load(esm);
    }

    // Find the given object ID, or return NULL if not found.
    const X* search(const std::string &id) const
    {
      std::string id2 = toLower (id);
      if(list.find(id2) == list.end())
        return NULL;
      return &list.find(id2)->second;
    }

    // Find the given object ID (throws an exception if not found)
    const X* find(const std::string &id) const
    {
        const X *object = search (id);

        if (!object)
            throw std::runtime_error ("object " + id + " not found");

        return object;
    }

    int getSize() { return list.size(); }
  };

    /// Modified version of RecListT for records, that need to store their own ID
  template <typename X>
  struct RecListWithIDT : RecList
  {
    typedef std::map<std::string,X> MapType;

    MapType list;

    // Load one object of this type
    void load(ESMReader &esm, const std::string &id)
    {
      std::string id2 = toLower (id);
      list[id2].load(esm, id2);
    }

    // Find the given object ID, or return NULL if not found.
    const X* search(const std::string &id) const
    {
      std::string id2 = toLower (id);
      if(list.find(id2) == list.end())
        return NULL;
      return &list.find(id2)->second;
    }

    // Find the given object ID (throws an exception if not found)
    const X* find(const std::string &id) const
    {
        const X *object = search (id);

        if (!object)
            throw std::runtime_error ("object " + id + " not found");

        return object;
    }

    int getSize() { return list.size(); }
  };

  // The only difference to the above is a slight change to the load()
  // function. We might merge these together later, and store the id
  // in all the structs.
  template <typename X>
  struct RecIDListT : RecList
  {
    typedef std::map<std::string,X> MapType;

    MapType list;

    void load(ESMReader &esm, const std::string &id)
    {
      std::string id2 = toLower (id);
      X& ref = list[id2];

      ref.id = id;
      ref.load(esm);
    }

    // Find the given object ID, or return NULL if not found.
    const X* search(const std::string &id) const
    {
      std::string id2 = toLower (id);
      if(list.find(id2) == list.end())
        return NULL;
      return &list.find(id2)->second;
    }

    // Find the given object ID (throws an exception if not found)
    const X* find(const std::string &id) const
    {
        const X *object = search (id);

        if (!object)
            throw std::runtime_error ("object " + id + " not found");

        return object;
    }

    int getSize() { return list.size(); }
  };

  // Cells aren't simply indexed by name. Exterior cells are treated
  // separately.
  // TODO: case handling (cell names are case-insensitive, but they are also showen to the
  // player, so we can't simply smash case.
  struct CellList : RecList
  {
    // Total cell count. Used for statistics.
    int count;
    CellList() : count(0) {}
    int getSize() { return count; }

    // List of interior cells. Indexed by cell name.
    typedef std::map<std::string,Cell*> IntCells;
    IntCells intCells;

    // List of exterior cells. Indexed as extCells[gridX][gridY].
    typedef std::map<int, Cell*> ExtCellsCol;
    typedef std::map<int, ExtCellsCol> ExtCells;
    ExtCells extCells;

    ~CellList()
    {
      for (IntCells::iterator it = intCells.begin(); it!=intCells.end(); ++it)
        delete it->second;

      for (ExtCells::iterator it = extCells.begin(); it!=extCells.end(); ++it)
      {
        ExtCellsCol& col = it->second;
        for (ExtCellsCol::iterator it = col.begin(); it!=col.end(); ++it)
        {
          delete it->second;
        }
      }
    }

    const Cell* findInt(const std::string &id) const
    {
      IntCells::const_iterator it = intCells.find(id);

      if(it == intCells.end())
        return NULL;

      return it->second;
    }

    const Cell *searchExt (int x, int y) const
    {
        ExtCells::const_iterator it = extCells.find (x);

        if (it==extCells.end())
            return 0;

        ExtCellsCol::const_iterator it2 = it->second.find (y);

        if (it2 == it->second.end())
            return 0;

        return it2->second;
    }

    const Cell *searchExtByName (const std::string& id) const
    {
        for (ExtCells::const_iterator iter = extCells.begin(); iter!=extCells.end(); ++iter)
        {
            const ExtCellsCol& column = iter->second;
            for (ExtCellsCol::const_iterator iter = column.begin(); iter!=column.end(); ++iter)
            {
                if (iter->second->name==id)
                    return iter->second;
            }
        }

        return 0;
    }

    const Cell *searchExtByRegion (const std::string& id) const
    {
        std::string id2 = toLower (id);

        for (ExtCells::const_iterator iter = extCells.begin(); iter!=extCells.end(); ++iter)
        {
            const ExtCellsCol& column = iter->second;
            for (ExtCellsCol::const_iterator iter = column.begin(); iter!=column.end(); ++iter)
            {
                if (toLower (iter->second->region)==id)
                    return iter->second;
            }
        }

        return 0;
    }

    void load(ESMReader &esm, const std::string &id)
    {
      using namespace std;

      count++;

      // All cells have a name record, even nameless exterior cells.
      Cell *cell = new Cell;
      cell->name = id;

      // The cell itself takes care of all the hairy details
      cell->load(esm);

      if(cell->data.flags & Cell::Interior)
        {
          // Store interior cell by name
          intCells[id] = cell;
        }
      else
        {
          // Store exterior cells by grid position
          extCells[cell->data.gridX][cell->data.gridY] = cell;
        }
    }
  };

  template <typename X>
  struct ScriptListT : RecList
  {
    typedef std::map<std::string,X> MapType;

    MapType list;

    // Load one object of this type
    void load(ESMReader &esm, const std::string &id)
    {
      X ref;
      ref.load (esm);

      std::string realId = toLower (ref.data.name.toString());

      std::swap (list[realId], ref);
    }

    // Find the given object ID, or return NULL if not found.
    const X* search(const std::string &id) const
    {
      std::string id2 = toLower (id);
      if(list.find(id2) == list.end())
        return NULL;
      return &list.find(id2)->second;
    }

    // Find the given object ID (throws an exception if not found)
    const X* find(const std::string &id) const
    {
        const X *object = search (id);

        if (!object)
            throw std::runtime_error ("object " + id + " not found");

        return object;
    }

    int getSize() { return list.size(); }
  };

  /* We need special lists for:

     Magic effects
     Skills
     Dialog / Info combo
     Scripts
     Land
     Path grids
     Land textures
  */
}
#endif
