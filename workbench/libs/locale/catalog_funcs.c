#include <libraries/locale.h>
#include "locale_intern.h"

/*
** Dispose the catalog's string but not the Catalog structure 
** itself.
*/
void dispose_catalog(struct IntCatalog * cat,
                     struct Library * LocaleBase)
{
  while (NULL != cat->ic_First)
  {
    struct CatStr * _catstr = cat->ic_First;
    cat->ic_First = _catstr->cs_Next;
    FreeVec(_catstr);
  }
}