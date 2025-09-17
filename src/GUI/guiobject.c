// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/*
* This file is part of the DZ09 project.
*
* Copyright (C) 2022 - 2019 AJScorp
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2 of the License.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/
#include "systemconfig.h"
#include "guiobject.h"

static pGUIOBJECT ActiveObject;

static void GUI_UpdateChildPositions(pGUIOBJECT Object, pPOINT dXY)
{
    pDLIST ChildList = &((pWIN)Object)->ChildObjects;

    if (DL_GetItemsCount(ChildList))
    {
        pDLITEM tmpItem = DL_GetFirstItem(ChildList);

        while (tmpItem != NULL)
        {
            pGUIOBJECT tmpObject = (pGUIOBJECT)tmpItem->Data;

            if (tmpObject != NULL)
            {
                tmpObject->Position.l += dXY->x;
                tmpObject->Position.r += dXY->x;
                tmpObject->Position.t += dXY->y;
                tmpObject->Position.b += dXY->y;

                if (GUI_IsWindowObject(tmpObject))
                    GUI_UpdateChildPositions(tmpObject, dXY);
            }
            tmpItem = DL_GetNextItem(tmpItem);
        }
    }
}

static pGUIOBJECT GUI_GetObjectRecursive(pGUIOBJECT Parent, pPOINT pt)
{
    pDLIST     ChildList = &((pWIN)Parent)->ChildObjects;
    pDLITEM    tmpDLItem = DL_GetLastItem(ChildList);
    pGUIOBJECT ResObject;

    while(tmpDLItem != NULL)
    {
        pGUIOBJECT tmpObject;

        ResObject = (pGUIOBJECT)tmpDLItem->Data;
        if ((ResObject != NULL) && ResObject->Visible &&
                IsPointInRect(pt, &ResObject->Position))
        {
            if (GUI_IsWindowObject(ResObject))
            {
                tmpObject = GUI_GetObjectRecursive(ResObject, pt);
                if (tmpObject != NULL) ResObject = tmpObject;
            }
            return ResObject;
        }
        tmpDLItem = DL_GetPrevItem(tmpDLItem);
    }
    return NULL;
}

static void GUI_UpdateChildTreeInheritance(pGUIOBJECT Object)
{
    pDLIST ChildList = &((pWIN)Object)->ChildObjects;

    if (DL_GetItemsCount(ChildList))
    {
        pDLITEM tmpItem = DL_GetFirstItem(ChildList);

        while (tmpItem != NULL)
        {
            pGUIOBJECT tmpObject = (pGUIOBJECT)tmpItem->Data;

            if (tmpObject != NULL)
            {
                tmpObject->InheritedEnabled = Object->Enabled && Object->InheritedEnabled;
                tmpObject->InheritedVisible = Object->Visible && Object->InheritedVisible;

                if (((uintptr_t)tmpObject == (uintptr_t)GUI_GetObjectActive()) &&
                        (!tmpObject->InheritedEnabled ||
                         !tmpObject->InheritedVisible))
                    GUI_SetObjectActive(NULL, false);

                if (GUI_IsWindowObject(tmpObject))
                    GUI_UpdateChildTreeInheritance(tmpObject);
            }
            tmpItem = DL_GetNextItem(tmpItem);
        }
    }
}

TRECT GUI_CalculateClientArea(pGUIOBJECT Object)
{
    TRECT ObjectRect;

    if ((Object != NULL) &&
            (Object->Type < GO_NUMTYPES) && (Object->Type != GO_UNKNOWN))
    {
        static void (*const CalcClientArea[GO_NUMTYPES])(pGUIOBJECT, pRECT) =
        {
            NULL,
            GUI_CalcClientAreaWindow,
            GUI_CalcClientAreaButton,
            NULL
        };

        if (CalcClientArea[Object->Type] != NULL)
            CalcClientArea[Object->Type](Object, &ObjectRect);
        else ObjectRect = Object->Position;
    }
    else ObjectRect = Rect(-1, -1, -1, -1);                                                         // UB!!!

    return ObjectRect;
}

pGUIOBJECT GUI_GetObjectFromPoint(pPOINT pt, pGUIOBJECT *RootParent)
{
    pGUIOBJECT Object = NULL, RootObject = NULL;

    if (pt != NULL)
    {
        int32_t i;

        for(i = LCDIF_NUMLAYERS - 1; i >= 0; i--)
        {
            TPOINT  tmpPoint;

            if ((GUILayer[i] == NULL) || !GUILayer[i]->Visible) continue;

            tmpPoint = GDI_ScreenToLayerPt(i, pt);

            if (IsPointInRect(&tmpPoint, &GUILayer[i]->Position))
            {
                pDLIST  ChildList;
                pDLITEM tmpDLItem;

                Object = RootObject = GUILayer[i];

                ChildList = &((pWIN)GUILayer[i])->ChildObjects;
                tmpDLItem = DL_GetLastItem(ChildList);
                while(tmpDLItem != NULL)
                {
                    pGUIOBJECT tmpRoot = (pGUIOBJECT)tmpDLItem->Data;

                    if ((tmpRoot != NULL) && tmpRoot->Visible &&
                            IsPointInRect(&tmpPoint, &tmpRoot->Position))
                    {
                        if ((Object = GUI_GetObjectRecursive(tmpRoot, &tmpPoint)) == NULL)
                            Object = tmpRoot;
                        RootObject = tmpRoot;
                        break;
                    }
                    tmpDLItem = DL_GetPrevItem(tmpDLItem);
                }
                break;
            }
        }
    }
    if (RootParent != NULL) *RootParent = RootObject;

    return Object;
}

pGUIOBJECT GUI_GetTopNonWindowObject(pGUIOBJECT Parent, pDLITEM *ObjectItem)
{
    pGUIOBJECT Result = NULL;
    pDLITEM    ResObjectItem = NULL;

    if ((Parent != NULL) && GUI_IsWindowObject(Parent))
    {
        pGUIOBJECT Object;
        pDLITEM    tmpDLItem;

        tmpDLItem = DL_GetLastItem(&((pWIN)Parent)->ChildObjects);
        while(tmpDLItem != NULL)
        {
            Object = (pGUIOBJECT)tmpDLItem->Data;
            if ((Object != NULL) && (Object->Type != GO_UNKNOWN) &&
                    !GUI_IsWindowObject(Object))
            {
                Result = Object;
                ResObjectItem = tmpDLItem;
                break;
            }
            tmpDLItem = DL_GetPrevItem(tmpDLItem);
        }
    }
    if (ObjectItem != NULL) *ObjectItem = ResObjectItem;

    return Result;
}

boolean GUI_SetObjectHandlerOnPress(pGUIOBJECT Object, void (*Handler)(pGUIOBJECT, pPOINT))
{
    if (Object != NULL)
    {
        Object->OnPress = Handler;
        return true;
    }
    else return false;
}

boolean GUI_SetObjectHandlerOnRelease(pGUIOBJECT Object, void (*Handler)(pGUIOBJECT, pPOINT))
{
    if (Object != NULL)
    {
        Object->OnRelease = Handler;
        return true;
    }
    else return false;
}

boolean GUI_SetObjectHandlerOnMove(pGUIOBJECT Object, void (*Handler)(pGUIOBJECT, pPOINT))
{
    if (Object != NULL)
    {
        Object->OnMove = Handler;
        return true;
    }
    else return false;
}

boolean GUI_SetObjectHandlerOnClick(pGUIOBJECT Object, void (*Handler)(pGUIOBJECT, pPOINT))
{
    if (Object != NULL)
    {
        Object->OnClick = Handler;
        return true;
    }
    else return false;
}

boolean GUI_SetObjectHandlerOnPaint(pGUIOBJECT Object, void (*Handler)(pGUIOBJECT, pRECT))
{
    if (Object != NULL)
    {
        Object->OnPaint = Handler;
        return true;
    }
    else return false;
}

boolean GUI_SetObjectHandlerOnDestroy(pGUIOBJECT Object, void (*Handler)(pGUIOBJECT))
{
    if (Object != NULL)
    {
        Object->OnDestroy = Handler;
        return true;
    }
    else return false;
}

boolean GUI_GetObjectPosition(pGUIOBJECT Object, pRECT Position)
{
    if (Object == NULL) return false;

    if (Position != NULL)
    {
        if (Object->Parent != NULL)
            *Position = GDI_GlobalToLocalRct(&Object->Position, &Object->Parent->Position.lt);
        else if (!LCDIF_GetLayerPosition(((pWIN)Object)->Layer, Position))
            return false;
    }
    return true;
}

boolean GUI_SetObjectPosition(pGUIOBJECT Object, pRECT Position)
{
    if ((Object == NULL) || (Position == NULL)) return false;

    if (Object->Parent == NULL)
    {
        TRECT   NewPosition = *Position;
        TRECT   OldPosition;
        boolean ChangedPitch, ChangedHeight;
        boolean Result;

        NORMALIZEVAL(NewPosition.l, NewPosition.r);
        NORMALIZEVAL(NewPosition.t, NewPosition.b);

        ChangedPitch = (Object->Position.r - Object->Position.l) != (NewPosition.r - NewPosition.l);
        ChangedHeight = (Object->Position.b - Object->Position.t) != (NewPosition.b - NewPosition.t);

        GUI_GetObjectPosition(Object, &OldPosition);
        OldPosition = GDI_GlobalToLocalRct(&OldPosition, &OldPosition.lt);

        Result = LCDIF_SetLayerPosition(((pWIN)Object)->Layer, NewPosition, true);
        if (Result)
        {
            Object->Position = GDI_GlobalToLocalRct(&NewPosition, &NewPosition.lt);

            if (Object->Visible)
            {
                if (ChangedPitch) GUI_Invalidate(Object, NULL);
                else if (ChangedHeight)
                {
                    TRECT  ScreenRect = GDI_LocalToGlobalRct(&OldPosition, &NewPosition.lt);
                    pRLIST UpdateRects = GDI_SUBRectangles(&Object->Position, &OldPosition);

                    ScreenRect = GDI_GlobalToLocalRct(&ScreenRect, &LCDScreen.ScreenOffset);
                    LCDIF_UpdateRectangle(ScreenRect);

                    if (UpdateRects != NULL)
                    {
                        uint32_t i;

                        for(i = 0; i < UpdateRects->Count; i++)
                            GUI_Invalidate(Object, &UpdateRects->Item[i]);

                        GDI_DeleteRList(UpdateRects);
                    }
                }
            }
        }
        return Result;
    }
    else
    {
        TRECT NewPosition = GDI_LocalToGlobalRct(Position, &Object->Parent->Position.lt);

        NORMALIZEVAL(NewPosition.l, NewPosition.r);
        NORMALIZEVAL(NewPosition.t,NewPosition.b);

        if (memcmp(&Object->Position, &NewPosition, sizeof(TRECT)) != 0)
        {
            TPOINT dXY = GDI_GlobalToLocalPt(&NewPosition.lt, &Object->Position.lt);
            pRLIST UpdateRects = GDI_SUBRectangles(&Object->Position, &NewPosition);

            Object->Position = NewPosition;

            if (GUI_IsWindowObject(Object))
                GUI_UpdateChildPositions(Object, &dXY);
            GUI_Invalidate(Object, NULL);

            if (UpdateRects != NULL)
            {
                uint32_t i;

                for(i = 0; i < UpdateRects->Count; i++)
                    GUI_Invalidate(Object->Parent, &UpdateRects->Item[i]);

                GDI_DeleteRList(UpdateRects);
            }
        }
    }
    return true;
}

boolean GUI_GetObjectEnabled(pGUIOBJECT Object)
{
    return ((Object != NULL) && Object->Enabled);
}

boolean GUI_SetObjectEnabled(pGUIOBJECT Object, boolean Enabled)
{
    if (Object == NULL) return false;
    if (Object->Enabled != Enabled)
    {
        Object->Enabled = Enabled;
        if (Object->Parent != NULL)
        {
            if (!Enabled && ((uintptr_t)Object == (uintptr_t)GUI_GetObjectActive()))
                GUI_SetObjectActive(NULL, false);

            if (GUI_IsWindowObject(Object))
                GUI_UpdateChildTreeInheritance(Object);

            GUI_Invalidate(Object, NULL);
        }
    }
    return true;
}

boolean GUI_GetObjectVisibility(pGUIOBJECT Object)
{
    return ((Object != NULL) && Object->Visible);
}

boolean GUI_SetObjectVisibility(pGUIOBJECT Object, boolean Visible)
{
    if (Object == NULL) return false;
    if (Object->Visible != Visible)
    {
        Object->Visible = Visible;

        if (!Visible && ((uintptr_t)Object == (uintptr_t)GUI_GetObjectActive()))
            GUI_SetObjectActive(NULL, false);

        if (Object->Parent == NULL)
        {
            if (GUI_IsWindowObject(Object))
            {
                GUI_UpdateChildTreeInheritance(Object);
                if (Visible)
                {
                    LCDIF_SetLayerEnabled(((pWIN)Object)->Layer, Visible, false);
                    GUI_Invalidate(Object, NULL);
                }
                else LCDIF_SetLayerEnabled(((pWIN)Object)->Layer, Visible, true);
            }
        }
        else
        {
            if (GUI_IsWindowObject(Object))
                GUI_UpdateChildTreeInheritance(Object);

            GUI_Invalidate(Object, NULL);
        }
    }
    return true;
}

pTEXT GUI_GetObjectText(pGUIOBJECT Object)
{
    if ((Object != NULL) && (Object->Type < GO_NUMTYPES))
    {
        static pTEXT (*const GetTextObject[GO_NUMTYPES])(pGUIOBJECT) =
        {
            NULL,
            NULL,
            GUI_GetTextButton,
            GUI_GetTextLabel
        };

        if (GetTextObject[Object->Type] != NULL)
            return GetTextObject[Object->Type](Object);
    }
    return NULL;
}

boolean GUI_SetObjectText(pGUIOBJECT Object, TTEXT ObjectText)
{
    boolean Result = false;

    if ((Object != NULL) && (Object->Type < GO_NUMTYPES))
    {
        static boolean (*const SetTextObject[GO_NUMTYPES])(pGUIOBJECT, pTEXT) =
        {
            NULL,
            NULL,
            GUI_SetTextButton,
            GUI_SetTextLabel
        };

        if (SetTextObject[Object->Type] != NULL)
        {
            GDI_UpdateTextExtent(&ObjectText);

            if ((Result = SetTextObject[Object->Type](Object, &ObjectText)) == true)
                GUI_Invalidate(Object, NULL);
        }
    }
    return Result;
}

pBFC_FONT GUI_GetObjectFont(pGUIOBJECT Object)
{
    if ((Object != NULL) && (Object->Type < GO_NUMTYPES))
    {
        static pTEXT (*const GetTextObject[GO_NUMTYPES])(pGUIOBJECT) =
        {
            NULL,
            NULL,
            GUI_GetTextButton,
            GUI_GetTextLabel
        };

        if (GetTextObject[Object->Type] != NULL)
        {
            pTEXT tmpText = GetTextObject[Object->Type](Object);

            return (tmpText != NULL) ? tmpText->Font : NULL;
        }
    }
    return NULL;
}

boolean GUI_SetObjectFont(pGUIOBJECT Object, pBFC_FONT ObjectFont)
{
    boolean Result = false;

    if ((Object != NULL) && (Object->Type < GO_NUMTYPES))
    {
        pTEXT  ObjectText;
        static pTEXT (*const GetTextObject[GO_NUMTYPES])(pGUIOBJECT) =
        {
            NULL,
            NULL,
            GUI_GetTextButton,
            GUI_GetTextLabel
        };

        if ((GetTextObject[Object->Type] != NULL) &&
                ((ObjectText = GetTextObject[Object->Type](Object)) != NULL))
        {
            ObjectText->Font = ObjectFont;
            GDI_UpdateTextExtent(ObjectText);

            GUI_Invalidate(Object, NULL);
            Result = true;
        }
    }
    return Result;
}

pTEXTCOLOR GUI_GetObjectTextColor(pGUIOBJECT Object)
{
    if ((Object != NULL) && (Object->Type < GO_NUMTYPES))
    {
        static pTEXT (*const GetTextObject[GO_NUMTYPES])(pGUIOBJECT) =
        {
            NULL,
            NULL,
            GUI_GetTextButton,
            GUI_GetTextLabel
        };

        if (GetTextObject[Object->Type] != NULL)
        {
            pTEXT tmpText = GetTextObject[Object->Type](Object);

            return (tmpText != NULL) ? &tmpText->Color : NULL;
        }
    }
    return NULL;
}

boolean GUI_SetObjecTextColor(pGUIOBJECT Object, TTEXTCOLOR Color)
{
    boolean Result = false;

    if ((Object != NULL) && (Object->Type < GO_NUMTYPES))
    {
        pTEXT  tmpText;
        static pTEXT (*const GetTextObject[GO_NUMTYPES])(pGUIOBJECT) =
        {
            NULL,
            NULL,
            GUI_GetTextButton,
            GUI_GetTextLabel
        };

        if ((GetTextObject[Object->Type] != NULL) &&
                ((tmpText = GetTextObject[Object->Type](Object)) != NULL))
        {
            TTEXT  ObjectText = *tmpText;
            static boolean (*const SetTextObject[GO_NUMTYPES])(pGUIOBJECT, pTEXT) =
            {
                NULL,
                NULL,
                GUI_SetTextButton,
                GUI_SetTextLabel
            };

            ObjectText.Color = Color;
            if (SetTextObject[Object->Type] != NULL)
                Result = SetTextObject[Object->Type](Object, &ObjectText);

            if (Result) GUI_Invalidate(Object, NULL);
        }
    }
    return Result;
}

char *GUI_GetObjectCaption(pGUIOBJECT Object)
{
    if ((Object != NULL) && (Object->Type < GO_NUMTYPES))
    {
        static pTEXT (*const GetTextObject[GO_NUMTYPES])(pGUIOBJECT) =
        {
            NULL,
            NULL,
            GUI_GetTextButton,
            GUI_GetTextLabel
        };

        if (GetTextObject[Object->Type] != NULL)
        {
            pTEXT tmpText = GetTextObject[Object->Type](Object);

            return (tmpText != NULL) ? tmpText->Text : NULL;
        }
    }
    return NULL;
}

boolean GUI_SetObjectCaption(pGUIOBJECT Object, char *Caption)
{
    boolean Result = false;

    if ((Object != NULL) && (Object->Type < GO_NUMTYPES))
    {
        pTEXT  ObjectText;
        static pTEXT (*const GetTextObject[GO_NUMTYPES])(pGUIOBJECT) =
        {
            NULL,
            NULL,
            GUI_GetTextButton,
            GUI_GetTextLabel
        };

        if ((GetTextObject[Object->Type] != NULL) &&
                ((ObjectText = GetTextObject[Object->Type](Object)) != NULL))
        {
            if ((ObjectText->Text != NULL) && IsDynamicMemory(ObjectText->Text))
                free(ObjectText->Text);

            ObjectText->Text = NULL;

            if ((Caption != NULL) && IsStackMemory(Caption))
            {
                uint32_t CaptionLength = strlen(Caption);
                char     *NewCaption = malloc(CaptionLength + 1);

                if (NewCaption != NULL)
                {
                    memcpy(NewCaption, Caption, CaptionLength);
                    NewCaption[CaptionLength] = '\0';
                }
                Caption = NewCaption;
            }
            ObjectText->Text = Caption;
            GDI_UpdateTextExtent(ObjectText);

            GUI_Invalidate(Object, NULL);
            Result = true;
        }
    }
    return Result;
}

pGUIOBJECT GUI_GetObjectActive(void)
{
    return ActiveObject;
}

void GUI_SetObjectActive(pGUIOBJECT Object, boolean Invalidate)
{
    if ((uintptr_t)Object != (uintptr_t)ActiveObject)
    {
        static void (*const SetActive[GO_NUMTYPES])(pGUIOBJECT, boolean) =
        {
            NULL,
            NULL,
            GUI_SetActiveButton,
            NULL
        };

        if (ActiveObject != NULL)
        {
            if (SetActive[ActiveObject->Type] != NULL)
            {
                SetActive[ActiveObject->Type](ActiveObject, false);
                if (Invalidate) GUI_Invalidate(ActiveObject, NULL);
            }
        }
        if ((Object->Type < GO_NUMTYPES) && (ActiveObject = Object) != NULL)
        {
            if (SetActive[ActiveObject->Type] != NULL)
            {
                SetActive[ActiveObject->Type](ActiveObject, true);
                if (Invalidate) GUI_Invalidate(ActiveObject, NULL);
            }
        }
        else ActiveObject = NULL;
    }
}

void GUI_UpdateActiveState(pGUIOBJECT Object, boolean Active)
{
    if ((ActiveObject != NULL) && ((uintptr_t)Object == (uintptr_t)ActiveObject))
    {
        static void (*const SetActive[GO_NUMTYPES])(pGUIOBJECT, boolean) =
        {
            NULL,
            NULL,
            GUI_SetActiveButton,
            NULL
        };
        static boolean (*const GetActive[GO_NUMTYPES])(pGUIOBJECT) =
        {
            NULL,
            NULL,
            GUI_GetActiveButton,
            NULL
        };

        if ((SetActive[ActiveObject->Type] != NULL) && (GetActive[ActiveObject->Type] != NULL) &&
                (GetActive[ActiveObject->Type](ActiveObject) != Active))
        {
            SetActive[ActiveObject->Type](ActiveObject, Active);
            GUI_Invalidate(ActiveObject, NULL);
        }
    }
}

void GUI_DrawObjectDefault(pGUIOBJECT Object, pRECT Clip)
{
    if ((Object != NULL) && (Object->Type < GO_NUMTYPES) && (Clip != NULL))
    {
        static void (*const DrawDefault[GO_NUMTYPES])(pGUIOBJECT, pRECT) =
        {
            NULL,
            GUI_DrawDefaultWindow,
            GUI_DrawDefaultButton,
            GUI_DrawDefaultLabel
        };

        if (DrawDefault[Object->Type] != NULL)
            DrawDefault[Object->Type](Object, Clip);
    }
}

void *GUI_DestroyObject(pGUIOBJECT Object)
{
    if ((Object != NULL) && (Object->Type != GO_UNKNOWN) && (Object->Type < GO_NUMTYPES))
    {
        TGODESTROYEV DestroyEvent = {0};

        GUI_SetObjectVisibility(Object, false);

        DestroyEvent.Object = Object;
        EM_PostEvent(ET_GODESTROY, NULL, &DestroyEvent, sizeof(TGODESTROYEV));
    }
    return NULL;
}
