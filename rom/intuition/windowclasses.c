/*
    (C) 1995-98 AROS - The Amiga Research OS
    $Id$

    Desc: Classes for window decor stuff, like dragbar, close etc.
    
    Lang: english
*/

#include <string.h>

#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/utility.h>
#include <proto/layers.h>

#include <intuition/classes.h>
#include <intuition/gadgetclass.h>
#include <intuition/cghooks.h>
#include <intuition/imageclass.h>
#include <aros/asmcall.h>

#include "gadgets.h"

#include "intuition_intern.h"

#undef SDEBUG
#undef DEBUG
#define SDEBUG 0
#define DEBUG 0
#include <aros/debug.h>


#define G(o)  ((struct Gadget *)o)
#define EG(o) ((struct ExtGadget *)o)
#define IM(o) ((struct Image *)o)

/* For table lookup of images */
#define SYSGADTYPE_IDX(o) ( ((G(o)->GadgetType & GTYP_SYSTYPEMASK) >> 4) - 1)

#define IntuitionBase ((struct IntuitionBase *)(cl)->cl_UserData)

const ULONG gtyp2image[] = 
{
    SIZEIMAGE,		/* GTYP_SIZING		*/
    0,			/* GTYP_WDRAGGING	*/
    0,			/* GTYP_SDRAGGING	*/
    DEPTHIMAGE,		/* GTYP_WDEPTH		*/
    SDEPTHIMAGE, 	/* GTYP_SDEPTH		*/
    ZOOMIMAGE,		/* GTYP_WZOOM		*/
    0,			/* GTYP_SUNUSED		*/
    CLOSEIMAGE		/* GTYP_CLOSE		*/
};

const WORD image_leftedge[] = 
{
    0,		/* GTYP_SIZING		*/
    0,		/* GTYP_WDRAGGING	*/
    0,		/* GTYP_SDRAGGING	*/
    -1,		/* GTYP_WDEPTH		*/
    -1, 	/* GTYP_SDEPTH		*/
    -1,		/* GTYP_WZOOM		*/
    0,		/* GTYP_SUNUSED		*/
    0,		/* GTYP_CLOSE		*/
};

struct dragbar_data
{
     /* Current left- and topedge of moving window. Ie when the user releases
     the LMB after a windowdrag, the window's new coords will be (curleft, curtop)
     */
     
     LONG curleft;
     LONG curtop;
     
     /* The current x and y coordinates relative to curleft/curtop */
     LONG mousex;
     LONG mousey;
     
     /* Whether the dragframe is currently drawn or erased */
     BOOL isrendered;
     
     /* Used to tell GM_GOINACTIVE whether the drag was canceled or not */
     BOOL drag_canceled;
     
     /* Rastport to use during update */
     struct RastPort *rp;
     
};
static VOID dragbar_render(Class *cl, Object *o, struct gpRender * msg)
{
    EnterFunc(bug("DragBar::Render()\n"));
    /* We will let the AROS gadgetclass test if it is safe to render */
    if ( DoSuperMethodA(cl, o, (Msg)msg) != 0)
    {
	struct DrawInfo *dri = msg->gpr_GInfo->gi_DrInfo;
        UWORD *pens = dri->dri_Pens;
	struct RastPort *rp = msg->gpr_RPort;
	struct IBox container;
	struct Window *win = msg->gpr_GInfo->gi_Window;
	struct TextExtent te;
	
	GetGadgetIBox(o, msg->gpr_GInfo, &container);
	
	if (container.Width <= 1 || container.Height <= 1)
	    return;
	    
	
	/* Clear the dragbar */
	
	SetAPen(rp, (win->Flags & WFLG_WINDOWACTIVE) ? 
			pens[FILLPEN] : pens[BACKGROUNDPEN]);
			
	SetDrMd(rp, JAM1);
	
	D(bug("Filling from (%d, %d) to (%d, %d)\n",
	    container.Left,
	    container.Top,
	    container.Left + container.Width - 1,
	    container.Top + container.Height - 1));
		
	RectFill(rp,
	    container.Left,
	    container.Top,
	    container.Left + container.Width - 1,
	    container.Top + container.Height - 2);
	    
	/* Draw a thin dark line around the bar */
	
	SetAPen(rp, pens[SHINEPEN]);
	RectFill(rp,container.Left,
		    container.Top,
		    container.Left,
		    container.Top + container.Height - 1 - ((container.Left == 0) ? 0 : 1));
	RectFill(rp,container.Left + 1,
		    container.Top,
		    container.Left + container.Width - 1,
		    container.Top);
	
	SetAPen(rp,pens[SHADOWPEN]);
	RectFill(rp,container.Left + container.Width - 1,
		    container.Top + 1,
		    container.Left + container.Width - 1,
		    container.Top + container.Height - 1);
	RectFill(rp,container.Left + ((container.Left == 0) ? 1 : 0),
		    container.Top + container.Height - 1,
		    container.Left + container.Width - 2,
		    container.Top + container.Height - 1);
	
	/* Render the titlebar */
	if (NULL != win->Title)
	{
	    ULONG textlen, titlelen;
	    
	    SetFont(rp, dri->dri_Font);

	    titlelen = strlen(win->Title);
	    textlen = TextFit(rp
		, win->Title
		, titlelen
		, &te
		, NULL
		, 1
		, container.Width
		, container.Height);

	    SetAPen(rp, pens[(win->Flags & WFLG_WINDOWACTIVE) ? FILLTEXTPEN : TEXTPEN]);
	    Move(rp, container.Left + 3, container.Top + dri->dri_Font->tf_Baseline + 3);
	
	    Text(rp, win->Title, textlen);
	}
	
    }  /* if (allowed to render) */
    
    ReturnVoid("DragBar::Render");
}

static IPTR dragbar_goactive(Class *cl, Object *o, struct gpInput *msg)
{
    
    IPTR retval = GMR_NOREUSE;
    
    struct InputEvent *ie = msg->gpi_IEvent;
    
    if (ie)
    {
    	/* The gadget was activated via mouse input */
	struct dragbar_data *data;
	struct Window *w;

	/* There is no point in rerendering ourseleves her, as this
	   is done by a call to RefreshWindowFrame() in the intuition inputhandler
	*/
    
	w = msg->gpi_GInfo->gi_Window;
    
	data = INST_DATA(cl, o);
	
	
	data->drag_canceled = FALSE;
	
	data->curleft = w->LeftEdge;
	data->curtop  = w->TopEdge;
    
	data->mousex = ie->ie_X - data->curleft;
	data->mousey = ie->ie_Y - data->curtop;
	
	data->rp = CloneRastPort(&w->WScreen->RastPort);
	if (data->rp)
	{
	
	
	    data->isrendered = FALSE;
	    
	    /* Lock all layers while the window is dragged */
D(bug("locking all layers\n"));
	    LockLayers(&w->WScreen->LayerInfo);
	    
	
	    retval = GMR_MEACTIVE;
	}
    }

    return retval;
    
    
}


static IPTR dragbar_handleinput(Class *cl, Object *o, struct gpInput *msg)
{
    IPTR retval = GMR_MEACTIVE;
    struct GadgetInfo *gi = msg->gpi_GInfo;
    
    if (gi)
    {
	struct InputEvent *ie = msg->gpi_IEvent;
	struct dragbar_data *data = INST_DATA(cl, o);
	struct Window *w = msg->gpi_GInfo->gi_Window;
	
	switch (ie->ie_Class)
	{
	case IECLASS_RAWMOUSE:
	    switch (ie->ie_Code)
	    {
	    case SELECTUP:
	    	retval = GMR_NOREUSE;
		break;
		    

	    case IECODE_NOBUTTON: {
	    	struct Screen *scr = w->WScreen;
		LONG new_left;
		LONG new_top;
		
	    
	    	/* Can we move to the new position, or is window at edge of display ? */
		new_left = ie->ie_X - data->mousex;
		new_top  = ie->ie_Y - data->mousey;
		
		if (new_left < 0)
		{
		    data->mousex += new_left;
		    new_left = 0;
		}
		
		if (new_top < 0)
		{
		    data->mousey += new_top;
		    new_top = 0;
		}
		
		if (new_left + w->Width > scr->Width)
		{
		    LONG correct_left;
		    correct_left = scr->Width - w->Width; /* align to screen border */
		    data->mousex += new_left - correct_left;
		    new_left = correct_left;
		}
		if (new_top + w->Height > scr->Height)
		{
		    LONG correct_top;
		    correct_top = scr->Height - w->Height; /* align to screen border */
		    data->mousey += new_top - correct_top;
		    new_top = correct_top;
		}
		
	    
		if (data->curleft != new_left || data->curtop != new_top)
		{
		    SetDrMd(data->rp, COMPLEMENT);



	    	    if (data->isrendered)
		    {
			/* Erase old frame */
			drawrect(data->rp
				, data->curleft
				, data->curtop
				, data->curleft + w->Width  - 1
				, data->curtop  + w->Height - 1
				, IntuitionBase
			);
			
		    }
		
		    data->curleft = new_left;
		    data->curtop  = new_top;
		     
		    /* Rerender the window frame */
		    
		    drawrect(data->rp
				, data->curleft
				, data->curtop
				, data->curleft + w->Width  - 1
				, data->curtop  + w->Height - 1
				, IntuitionBase
		    );
		    
		    data->isrendered = TRUE;
		
		     
		}
		
		retval = GMR_MEACTIVE;
		
		break; }
		
	    default:
	    	retval = GMR_REUSE;
		data->drag_canceled = TRUE;
		break;
	    	
	    
	    
	    } /* switch (ie->ie_Code) */
	    
	    
	} /* switch (ie->ie_Class) */
	
    } /* if (gi) */
    return retval;
}

static IPTR dragbar_goinactive(Class *cl, Object *o, struct gpGoInactive *msg)
{
    struct dragbar_data *data;
    struct Window *w;
    
    data = INST_DATA(cl, o);
    w = msg->gpgi_GInfo->gi_Window;
    
    /* Allways clear last drawn frame */
		
    if (data->curleft != w->LeftEdge || data->curtop != w->TopEdge)
    {
		    
	if (data->isrendered)
	{

	    SetDrMd(data->rp, COMPLEMENT);
	    
	    /* Erase old frame */
	    drawrect(data->rp
		, data->curleft
		, data->curtop
		, data->curleft + w->Width  - 1
		, data->curtop  + w->Height - 1
		, IntuitionBase
	    );
			
	}
	
    }
    if (!data->drag_canceled)
    {
	    
		
	MoveWindow(w
	    , data->curleft - w->LeftEdge	/* dx */
	    , data->curtop  - w->TopEdge	/* dy */
	);
		
    }
    data->drag_canceled = FALSE;
		    
		
    /* User throught with drag operation. Unlock layesr and free
	   rastport clone
    */
    UnlockLayers(&w->WScreen->LayerInfo);
    FreeRastPort(data->rp);
    
    return TRUE;
	
    
}

/***********  Window dragbar class **********************************/



AROS_UFH3S(IPTR, dispatch_dragbarclass,
    AROS_UFHA(Class *,  cl,  A0),
    AROS_UFHA(Object *, o,   A2),
    AROS_UFHA(Msg,      msg, A1)
)
{
    IPTR retval = 0UL;
    
    EnterFunc(bug("dragbar_dispatcher(mid=%d)\n", msg->MethodID));
    switch (msg->MethodID)
    {
	case GM_RENDER:
	    dragbar_render(cl, o, (struct gpRender *)msg);
	    break;
	    
	case GM_LAYOUT:
	    break;
	    
	case GM_DOMAIN:
	    break;
	    
	case GM_GOACTIVE:
	    retval = dragbar_goactive(cl, o, (struct gpInput *)msg);
	    break;
	    
	case GM_GOINACTIVE:
	    retval = dragbar_goinactive(cl, o, (struct gpGoInactive *)msg);
	    break;
	    
	case GM_HANDLEINPUT:
	    retval = dragbar_handleinput(cl, o, (struct gpInput *)msg);
	    break;
	    
	case OM_NEW:
	    retval = DoSuperMethodA(cl, o, msg);
	    if (NULL != retval) {
	    	((struct Gadget *)retval)->GadgetType |= GTYP_SYSGADGET;
	    }
	    break;
	
	    
	default:
	    retval = DoSuperMethodA(cl, o, msg);
	    break;
    }
    
    ReturnPtr ("dragbar_dispatcher", IPTR, retval);
}

/*********************
** The SizeButtonClass
*********************/


struct sizebutton_data
{

     /* The current width and height of the rubber band frame */
     ULONG width;
     ULONG height;
     
     /* the offset of the mouse pointer to the rubber band frame*/
     LONG mouseoffsetx;
     LONG mouseoffsety;
     
     /* Whether the dragframe is currently drawn or erased */
     BOOL isrendered;
     
     /* Used to tell GM_GOINACTIVE whether the drag was canceled or not */
     BOOL drag_canceled;
     
     /* Rastport to use during update */
     struct RastPort *rp;
     
     Object * image;
};

static VOID sizebutton_render(Class *cl, Object *o, struct gpRender * msg)
{
	struct sizebutton_data *data = INST_DATA(cl, o);
	struct IBox container;
	struct RastPort *rp = msg->gpr_RPort;
	
	/* center image position, we assume image top and left is 0 */
	ULONG x, y;
        ULONG state;
	UWORD *pens = msg->gpr_GInfo->gi_DrInfo->dri_Pens;
	
	GetGadgetIBox(o, msg->gpr_GInfo, &container);
	D(bug("Gadget IBOX\n"));

	x = container.Left + ((container.Width / 2) -
		    (IM(data->image)->Width / 2));
		    
	y = container.Top + ((container.Height / 2) -
		    (IM(data->image)->Height / 2));
		
	/* Are we part of the active window ? */
	if (msg->gpr_GInfo->gi_Window->Flags & WFLG_WINDOWACTIVE)
	{
	    state = ( (G(o)->Flags & GFLG_SELECTED) ? IDS_SELECTED : IDS_NORMAL );
	}
	else
	{
	    state = ( (G(o)->Flags & GFLG_SELECTED) ? IDS_INACTIVESELECTED : IDS_INACTIVENORMAL );
	}
	
	D(bug("Drawing image\n"));

   	DrawImageState(rp
	    , (struct Image *)data->image
	    , x, y
	    , state
	    , msg->gpr_GInfo->gi_DrInfo);
	    
	/* For now just render a tiny black edge around the image */

	SetAPen(rp, pens[((state == IDS_SELECTED) || (state == IDS_INACTIVESELECTED)) ? SHADOWPEN : SHINEPEN]);
	RectFill(rp,container.Left,
		    container.Top,
		    container.Left,
		    container.Top + container.Height - 1 - ((container.Left == 0) ? 0 : 1));
	RectFill(rp,container.Left + 1,
		    container.Top,
		    container.Left + container.Width - 1,
		    container.Top);
	
	SetAPen(rp, pens[((state == IDS_SELECTED) || (state == IDS_INACTIVESELECTED)) ? SHINEPEN : SHADOWPEN]);
	RectFill(rp,container.Left + container.Width - 1,
		    container.Top + 1,
		    container.Left + container.Width - 1,
		    container.Top + container.Height - 1);
	RectFill(rp,container.Left + ((container.Left == 0) ? 1 : 0),
		    container.Top + container.Height - 1,
		    container.Left + container.Width - 2,
		    container.Top + container.Height - 1);

    return;
}

static IPTR sizebutton_goactive(Class *cl, Object *o, struct gpInput *msg)
{
    
    IPTR retval = GMR_NOREUSE;
    
    struct InputEvent *ie = msg->gpi_IEvent;
    
    if (ie)
    {
    	/* The gadget was activated via mouse input */
	struct sizebutton_data *data;
	struct Window *w;

	/* There is no point in rerendering ourseleves her, as this
	   is done by a call to RefreshWindowFrame() in the intuition inputhandler
	*/
    
	w = msg->gpi_GInfo->gi_Window;
    
	data = INST_DATA(cl, o);
	
	
	data->drag_canceled = FALSE;
	
	data->height = w->Height;
	data->width  = w->Width;
        
        data->mouseoffsetx = w->Width  - (ie->ie_X - w->LeftEdge);
        data->mouseoffsety = w->Height - (ie->ie_Y - w->TopEdge);

	data->rp = CloneRastPort(&w->WScreen->RastPort);
	if (data->rp)
	{
	
	
	    data->isrendered = FALSE;
	    
	    /* Lock all layers while the window is resized */
D(bug("locking all layers\n"));
	    LockLayers(&w->WScreen->LayerInfo);
	    
	
	    retval = GMR_MEACTIVE;
	}
    }

    return retval;
    
    
}


static IPTR sizebutton_handleinput(Class *cl, Object *o, struct gpInput *msg)
{
    IPTR retval = GMR_MEACTIVE;
    struct GadgetInfo *gi = msg->gpi_GInfo;
    
    if (gi)
    {
	struct InputEvent *ie = msg->gpi_IEvent;
	struct sizebutton_data *data = INST_DATA(cl, o);
	struct Window *w = msg->gpi_GInfo->gi_Window;
	
	switch (ie->ie_Class)
	{
	case IECLASS_RAWMOUSE:
	    switch (ie->ie_Code)
	    {
	    case SELECTUP:
	    	retval = GMR_NOREUSE;
		break;
		    

	    case IECODE_NOBUTTON: {
	    	struct Screen *scr = w->WScreen;
		LONG new_width;
		LONG new_height;
		
	    
	    	/* Can we move to the new position, or is window at edge of display ? */
		new_width   = ie->ie_X - w->LeftEdge + data->mouseoffsetx;
		new_height  = ie->ie_Y - w->TopEdge  + data->mouseoffsety;
		
		if (new_width < 0)
		  new_width = 1;
		
		if (w->MinWidth != 0 && new_width < (ULONG)w->MinWidth)
		  new_width = w->MinWidth;
		  
		if (w->MaxWidth != 0 && new_width > (ULONG)w->MaxWidth)
		  new_width = w->MaxWidth;
		
		if (new_height < 0)
		  new_height = 1;
		
		if (w->MinHeight != 0 && new_height < (ULONG)w->MinHeight)
		  new_height = w->MinHeight;
		  
		if (w->MaxHeight != 0 && new_height > (ULONG)w->MaxHeight)
		  new_height = w->MaxHeight;


                /* limit dimensions so window fits on the screen */		
		if (new_width + w->LeftEdge > scr->Width)
		  new_width = scr->Width - w->LeftEdge;
		
		if (new_height + w->TopEdge > scr->Height)
		  new_height = scr->Height - w->TopEdge;
		

		if (data->height != new_height || data->width != new_width)
		{
		    SetDrMd(data->rp, COMPLEMENT);

	    	    if (data->isrendered)
		    {
			/* Erase old frame */
			drawrect(data->rp
				, w->LeftEdge
				, w->TopEdge
				, w->LeftEdge + data->width  - 1
				, w->TopEdge  + data->height - 1
				, IntuitionBase
			);
			
		    }
		
		    data->width   = new_width;
		    data->height  = new_height;
		     
		    /* Rerender the window frame */
		    
  		    drawrect(data->rp
				, w->LeftEdge
				, w->TopEdge
				, w->LeftEdge + data->width  - 1
				, w->TopEdge  + data->height - 1
				, IntuitionBase
			);
		    
		    data->isrendered = TRUE;
		
		     
		}
		
		retval = GMR_MEACTIVE;
		
		break; }
		
	    default:
	    	retval = GMR_REUSE;
		data->drag_canceled = TRUE;
		break;
	    	
	    
	    
	    } /* switch (ie->ie_Code) */
	    
	    
	} /* switch (ie->ie_Class) */
	
    } /* if (gi) */
    return retval;
}

static IPTR sizebutton_goinactive(Class *cl, Object *o, struct gpGoInactive *msg)
{
    struct sizebutton_data *data;
    struct Window *w;
    
    data = INST_DATA(cl, o);
    w = msg->gpgi_GInfo->gi_Window;
    
    /* Allways clear last drawn frame */
		
    if (data->width != w->Width || data->height != w->Height)
    {
		    
	if (data->isrendered)
	{

	    SetDrMd(data->rp, COMPLEMENT);
	    
	    /* Erase old frame */
	    drawrect(data->rp
		, w->LeftEdge
		, w->TopEdge
		, w->LeftEdge + data->width  - 1
		, w->TopEdge  + data->height - 1
		, IntuitionBase
	    );
			
	}
	
    }
    if (!data->drag_canceled)
    {
	    
		
	SizeWindow(w
	    , data->width  - w->Width	/* dx */
	    , data->height - w->Height	/* dy */
	);
		
    }
    data->drag_canceled = FALSE;
		    
		
    /* User throught with drag operation. Unlock layesr and free
	   rastport clone
    */
    UnlockLayers(&w->WScreen->LayerInfo);
    FreeRastPort(data->rp);
    
    return TRUE;
}

static Object *sizebutton_new(Class *cl, Object *o, struct opSet *msg)
{
    /* Only interesting attribute is gadget type */
    
    o = (Object *)DoSuperMethodA(cl, o, (Msg)msg);
    if (o)
    {
    	struct sizebutton_data *data = INST_DATA(cl, o);
	ULONG dispose_mid = OM_DISPOSE;
	struct DrawInfo *dri;
	
	memset(data, 0, sizeof (struct sizebutton_data));
	
	dri = (struct DrawInfo *)GetTagData(GA_DrawInfo, NULL, msg->ops_AttrList);
	if (dri)
	{
	    struct TagItem image_tags[] =
	    {
/*	    	{IA_Left,		image_leftedge[SYSGADTYPE_IDX(o)]				},*/
	    	{IA_Width,		G(o)->Width - 2 /* - image_leftedge[SYSGADTYPE_IDX(o)] */	},
	    	{IA_Height, 		G(o)->Height - 2						},
	    	{SYSIA_Which,		gtyp2image[SYSGADTYPE_IDX(o)]					},
	    	{SYSIA_DrawInfo,	(IPTR)dri							},
	    	{TAG_DONE, 0UL}
	    };

	    /* Create a sysimage with gadget's sizes */
	    data->image = NewObjectA(NULL, SYSICLASS, image_tags);
	    D(bug("tbb: image=%p\n", data->image));
	    if (data->image)
	    {
	    	((struct Gadget *)o)->GadgetType |= GTYP_SYSGADGET;
	   	return o;
	    }
	} /* if (dri) */
	CoerceMethodA(cl, o, (Msg)&dispose_mid);
	
    } /* if (o) */
    
    return NULL;
}

static VOID sizebutton_dispose(Class *cl, Object *o, Msg msg)
{
    struct sizebutton_data *data = INST_DATA(cl, o);
    if (data->image)
    	DisposeObject(data->image);
    DoSuperMethodA(cl, o, msg);
    return;
}

/***********  Size Button class **********************************/



AROS_UFH3S(IPTR, dispatch_sizebuttonclass,
    AROS_UFHA(Class *,  cl,  A0),
    AROS_UFHA(Object *, o,   A2),
    AROS_UFHA(Msg,      msg, A1)
)
{
    IPTR retval = 0UL;

    EnterFunc(bug("sizebutton_dispatcher(mid=%d)\n", msg->MethodID));
    switch (msg->MethodID)
    {
	case GM_RENDER:
	    sizebutton_render(cl, o, (struct gpRender *)msg);
	    break;
	    
	case GM_LAYOUT:
	    break;
	    
	case GM_DOMAIN:
	    break;
	    
	case GM_GOACTIVE:
	    retval = sizebutton_goactive(cl, o, (struct gpInput *)msg);
	    break;
	    
	case GM_GOINACTIVE:
	    retval = sizebutton_goinactive(cl, o, (struct gpGoInactive *)msg);
	    break;
	    
	case GM_HANDLEINPUT:
	    retval = sizebutton_handleinput(cl, o, (struct gpInput *)msg);
	    break;
	    
	case OM_NEW:
	    retval = (IPTR)sizebutton_new(cl, o, (struct opSet *)msg);
	    break;
	    
	case OM_DISPOSE:
	    sizebutton_dispose(cl, o, msg);
	    break;
	
	    
	default:
	    retval = DoSuperMethodA(cl, o, msg);
	    break;
    }
    
    ReturnPtr ("sizebutton_dispatcher", IPTR, retval);
}



/************************************************
*  TBButton class (TitleBarButton)
*************************************************/

struct tbb_data
{
    Object *image;
};
/* This class handles all buttons in titlebar, ie. Close, depth & zoom gadgets.
 Note: I could put these in separate subclasses, but thhey have
 so much in common, that I just have them in the same class
*/

static Object *tbb_new(Class *cl, Object *o, struct opSet *msg)
{
    /* Only interesting attribute is gadget type */
    
    o = (Object *)DoSuperMethodA(cl, o, (Msg)msg);
    if (o)
    {
    	struct tbb_data *data = INST_DATA(cl, o);
	ULONG dispose_mid = OM_DISPOSE;
	struct DrawInfo *dri;
	
	memset(data, 0, sizeof (struct tbb_data));
	
	dri = (struct DrawInfo *)GetTagData(GA_DrawInfo, NULL, msg->ops_AttrList);
	if (dri)
	{
	    struct TagItem image_tags[] =
	    {
/*	    	{IA_Left,		image_leftedge[SYSGADTYPE_IDX(o)]				},*/
	    	{IA_Width,		G(o)->Width - 2 /* - image_leftedge[SYSGADTYPE_IDX(o)] */	},
	    	{IA_Height, 		G(o)->Height - 2						},
	    	{SYSIA_Which,		gtyp2image[SYSGADTYPE_IDX(o)]					},
	    	{SYSIA_DrawInfo,	(IPTR)dri							},
	    	{TAG_DONE, 0UL}
	    };

	    /* Create a sysimage with gadget's sizes */
	    data->image = NewObjectA(NULL, SYSICLASS, image_tags);
	    D(bug("tbb: image=%p\n", data->image));
	    if (data->image)
	    {
	    	((struct Gadget *)o)->GadgetType |= GTYP_SYSGADGET;
	   	return o;
	    }
	} /* if (dri) */
	CoerceMethodA(cl, o, (Msg)&dispose_mid);
	
    } /* if (o) */
    
    return NULL;
}

static VOID tbb_dispose(Class *cl, Object *o, Msg msg)
{
    struct tbb_data *data = INST_DATA(cl, o);
    if (data->image)
    	DisposeObject(data->image);
    DoSuperMethodA(cl, o, msg);
    return;
}


static VOID tbb_render(Class *cl, Object *o, struct gpRender *msg)
{
	struct tbb_data *data = INST_DATA(cl, o);
	struct IBox container;
	struct RastPort *rp = msg->gpr_RPort;
	
	/* center image position, we assume image top and left is 0 */
	ULONG x, y;
        ULONG state;
	UWORD *pens = msg->gpr_GInfo->gi_DrInfo->dri_Pens;
	
	GetGadgetIBox(o, msg->gpr_GInfo, &container);
	D(bug("Gadget IBOX\n"));

	x = container.Left + ((container.Width / 2) -
		    (IM(data->image)->Width / 2));
		    
	y = container.Top + ((container.Height / 2) -
		    (IM(data->image)->Height / 2));
		
	/* Are we part of the active window ? */
	if (msg->gpr_GInfo->gi_Window->Flags & WFLG_WINDOWACTIVE)
	{
	    state = ( (G(o)->Flags & GFLG_SELECTED) ? IDS_SELECTED : IDS_NORMAL );
	}
	else
	{
	    state = ( (G(o)->Flags & GFLG_SELECTED) ? IDS_INACTIVESELECTED : IDS_INACTIVENORMAL );
	}
	
	D(bug("Drawing image\n"));

   	DrawImageState(rp
	    , (struct Image *)data->image
	    , x, y
	    , state
	    , msg->gpr_GInfo->gi_DrInfo);
	    
	/* For now just render a tiny black edge around the image */

	SetAPen(rp, pens[((state == IDS_SELECTED) || (state == IDS_INACTIVESELECTED)) ? SHADOWPEN : SHINEPEN]);
	RectFill(rp,container.Left,
		    container.Top,
		    container.Left,
		    container.Top + container.Height - 1 - ((container.Left == 0) ? 0 : 1));
	RectFill(rp,container.Left + 1,
		    container.Top,
		    container.Left + container.Width - 1,
		    container.Top);
	
	SetAPen(rp, pens[((state == IDS_SELECTED) || (state == IDS_INACTIVESELECTED)) ? SHINEPEN : SHADOWPEN]);
	RectFill(rp,container.Left + container.Width - 1,
		    container.Top + 1,
		    container.Left + container.Width - 1,
		    container.Top + container.Height - 1);
	RectFill(rp,container.Left + ((container.Left == 0) ? 1 : 0),
		    container.Top + container.Height - 1,
		    container.Left + container.Width - 2,
		    container.Top + container.Height - 1);
    return;
}


static IPTR tbb_handleinput(Class *cl, Object *o, struct gpInput *msg)
{
    IPTR retval;
    
    /* Let the buttong superclass handle the input for us */
    retval = DoSuperMethodA(cl, o, (Msg)msg);
    
    /* Now, what exactly did the user do ? */
    if (retval != GMR_MEACTIVE)
    {
    	/* Button no longer active. But was the users button release inside the gadget ?. */
	if (retval & GMR_VERIFY)
	{
	    /* The mousebutton release was inside gadget.
	    Now perform the button's action */
	    
	    
	    switch (EG(o)->GadgetType & GTYP_SYSTYPEMASK)
	    {
		case GTYP_CLOSE: {
		    struct IntuiMessage *imsg;
		    
		    
		    /* Send an IDCMP_CLOSEWINDOW message to the application */
		    /* Get an empty intuimessage */
		    imsg = alloc_intuimessage(IntuitionBase);
		    if (imsg)
		    {
			/* Fill it in */
			imsg->Class	= IDCMP_CLOSEWINDOW;
			imsg->Code	= 0;
			imsg->Qualifier = 0;
			imsg->IAddress	= 0;
			imsg->MouseX	= 0;
			imsg->MouseY	= 0;
			imsg->Seconds	= 0;
			imsg->Micros	= 0;
			
			/* And send it to the application */
			send_intuimessage(imsg, msg->gpi_GInfo->gi_Window, IntuitionBase);
			
		    }
		    
		    break; }
		    
		case GTYP_WDEPTH: {
		    struct Window *w = msg->gpi_GInfo->gi_Window;
		    
		    
#warning How is window refreshing handled here ? Ie when is IDCMP_REFRESHWINDOW sent ?
		    
		    LockLayerInfo(&w->WScreen->LayerInfo);

		    if (NULL == w->WLayer->front)
		    {
		    	/* Send window to back */
			WindowToBack(w);
		    }
		    else
		    {
		    	/* Send window to front */
			WindowToFront(w);
		    }
		    UnlockLayerInfo(&w->WScreen->LayerInfo);
		    
		    break; }
		
		case GTYP_WZOOM:
		    ZipWindow(msg->gpi_GInfo->gi_Window);
		    break;
	    }
	    
	    
	} /* if (verified button press) */
	
    } /* if (gadget no longer active) */
    
    return retval;
    
}


AROS_UFH3S(IPTR, dispatch_tbbclass,
    AROS_UFHA(Class *,  cl,  A0),
    AROS_UFHA(Object *, o,   A2),
    AROS_UFHA(Msg,      msg, A1)
)
{
    IPTR retval = 0UL;
    
    EnterFunc(bug("tbb_dispatcher(mid=%d)\n", msg->MethodID));
    switch (msg->MethodID)
    {
	case OM_NEW:
	    retval = (IPTR)tbb_new(cl, o, (struct opSet *)msg);
	    break;
	    
	case OM_DISPOSE:
	    tbb_dispose(cl, o, msg);
	    break;
	    
	case GM_RENDER:
	    tbb_render(cl, o, (struct gpRender *)msg);
	    break;
	    
	case GM_LAYOUT:
	    break;
	    
	case GM_DOMAIN:
	    break;
	    

	case GM_HANDLEINPUT:
	    retval = tbb_handleinput(cl, o , (struct gpInput *)msg);
	    break;
	    
	    
	default:
	    retval = DoSuperMethodA(cl, o, msg);
	    break;
    }
    
    ReturnPtr ("tbb_dispatcher", IPTR, retval);
}

/****************************************************************************/
#undef IntuitionBase

/* Initialize our dragbar class. */
struct IClass *InitDragBarClass (struct IntuitionBase * IntuitionBase)
{
    struct IClass *cl = NULL;

    /* This is the code to make the dragbarclass...
    */
    if ( (cl = MakeClass(NULL, GADGETCLASS, NULL, sizeof (struct dragbar_data), 0)) )
    {
	cl->cl_Dispatcher.h_Entry    = (APTR)AROS_ASMSYMNAME(dispatch_dragbarclass);
	cl->cl_Dispatcher.h_SubEntry = NULL;
	cl->cl_UserData 	     = (IPTR)IntuitionBase;

    }

    return (cl);
}


/* Initialize our sizebutton class. */
struct IClass *InitSizeButtonClass (struct IntuitionBase * IntuitionBase)
{
    struct IClass *cl = NULL;

    /* This is the code to make the dragbarclass...
    */
    if ( (cl = MakeClass(NULL, GADGETCLASS, NULL, sizeof (struct sizebutton_data), 0)) )
    {
	cl->cl_Dispatcher.h_Entry    = (APTR)AROS_ASMSYMNAME(dispatch_sizebuttonclass);
	cl->cl_Dispatcher.h_SubEntry = NULL;
	cl->cl_UserData 	     = (IPTR)IntuitionBase;
    }

    return (cl);
}


/* Initialize our TitleBarButton class. */
struct IClass *InitTitleBarButClass (struct IntuitionBase * IntuitionBase)
{
    struct IClass *cl = NULL;

    /* This is the code to make the tbbclass...
    */
    if ( (cl = MakeClass(NULL, BUTTONGCLASS, NULL, sizeof (struct tbb_data), 0)) )
    {
	cl->cl_Dispatcher.h_Entry    = (APTR)AROS_ASMSYMNAME(dispatch_tbbclass);
	cl->cl_Dispatcher.h_SubEntry = NULL;
	cl->cl_UserData 	     = (IPTR)IntuitionBase;

    }

    return (cl);
}

