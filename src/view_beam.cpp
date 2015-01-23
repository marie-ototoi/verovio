/////////////////////////////////////////////////////////////////////////////
// Name:        view_beam.cpp
// Author:      Laurent Pugin
// Created:     2005
// Copyright (c) Author and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////


#include "view.h"

//----------------------------------------------------------------------------

#include <algorithm>
#include <assert.h>
#include <iostream>
#include <math.h>

//----------------------------------------------------------------------------

#include "beam.h"
#include "doc.h"
#include "layer.h"
#include "layerelement.h"
#include "note.h"
#include "staff.h"

namespace vrv {

#define MAX_ELEMENTS_IN_BEAM 80		/* nombre de valeurs reliees possibles */
#define MAX_MIF 20	/* nombre max de sous-groupes de beams */

struct BeamElementCoord {
    int x;
    int y; // represents the point farthest from the beam
    int yMax; // used if representing a chord
    int yMin; // used if representing a chord
    int yBeam; // height of stems
    int dur; // drawing duration
    int breaksec;
    LayerElement *element;
};

void View::DrawBeamPostponed( DeviceContext *dc, Layer *layer, Beam *beam, Staff *staff )
{
    LayerElement *current;
    
    BeamElementCoord beamElementCoord[MAX_ELEMENTS_IN_BEAM]; /* garde les coord.d'entree*/

	bool changingDur = OFF;
    bool hasChord = OFF;
    data_STEMDIRECTION stemDir = STEMDIRECTION_NONE;
    
    // position variables
    int high, low;
    int verticalCenter;
    int yExtreme;
    int height=0;
    
    double verticalShiftFactor = 3.0;
    
    // For slope calculation and linear regression
	double slope = 0.0;
    double sy_up = 0.0;
    double dA, dB;
    double s_x=0.0, s_y=0.0, s_xy=0.0, s_x2=0.0, s_y2=0.0;
    double xr;
    
    // position in the beam element list
	int currentPos, previousPos;
    
    // duration variables
	int shortestDur, lastDur, currentDur, testDur;

    // position x for the stem (normal and cue size)
	int dx[2];
    
    // temporary coordinates
    int fx1,fx2,fy1,fy2;
    
    // temporary variables
	int avgY, shiftY, barY, verticalShift, y1, fbarre_y, fullBars, polygonHeight;
    
	int mx_i[MAX_MIF], mx_f[MAX_MIF], my_i[MAX_MIF], my_f[MAX_MIF];
	char m_i[MAX_MIF];	/* pour stocker les prov des marqueurs */

    // loops
	int i, j, k, t, si;		/* compteurs de boucles */
	int _mif;
	int apax;
    
    // beam bar sizes
	int beamWidth, beamWidthBlack, beamWidthWhite;

    
    /******************************************************************/
    // initialization
    
    shortestDur = 0;
    lastDur = currentPos = 0;
    high = avgY = sy_up = 0.0;
    
    verticalCenter = staff->GetDrawingY() - (m_doc->m_drawingInterl[staff->staffSize] * 2); //center point of the staff
    yExtreme = verticalCenter; //value of farthest y point on the staff from verticalCenter minus verticalCenter; used if hasChord = ON

    beam->ResetList( beam );
    
    // Should we assert this at the beginning?
    if (beam->m_list.empty()) {
        return;
    }
    
    assert( beam->m_list.size() < MAX_ELEMENTS_IN_BEAM );
    
    // current point to the first Note in the layed out layer
    current = dynamic_cast<LayerElement*>(beam->m_list.front());
    // Beam list should contain only DurationInterface objects
    assert( dynamic_cast<DurationInterface*>(current) );
    
	low = current->GetDrawingY();
    lastDur = dynamic_cast<DurationInterface*>(current)->GetDur();

    dx[0] =  m_doc->m_drawingNoteRadius[staff->staffSize][0];
    dx[1] =  m_doc->m_drawingNoteRadius[staff->staffSize][1];
    dx[0] -= (m_doc->m_env.m_stemWidth)/2;
    dx[1] -= (m_doc->m_env.m_stemWidth)/2;
    
    /******************************************************************/
    // Populate BeamElementCoord for each element in the beam
    
    ListOfObjects::iterator iter = beam->m_list.begin();
	do {
        // Beam list should contain only DurationInterface objects
        assert( dynamic_cast<DurationInterface*>(current) );
        
        currentDur = dynamic_cast<DurationInterface*>(current)->GetDur();
        
        if ( current->IsChord() ) {
            hasChord = true;
        }

        // Can it happen? With rests?
		if (currentDur > DUR_4) {
			beamElementCoord[currentPos].element = current;
            beamElementCoord[currentPos].x = current->GetDrawingX();
			beamElementCoord[currentPos].dur = currentDur;
            
            // Look at beam breaks
            beamElementCoord[currentPos].breaksec = -1;
            AttBeamsecondary *beamsecondary = dynamic_cast<AttBeamsecondary*>(current);
            if ( currentPos && beamsecondary && beamsecondary->HasBreaksec()) {
                beamElementCoord[currentPos].breaksec = beamsecondary->GetBreaksec();
            }
            
            // Skip rests
            if (current->IsNote() || current->IsChord()) {
                // keep the shortest dur in the beam
                shortestDur = std::max(currentDur,shortestDur);
                // check if we have more than duration in the beam
                if (!changingDur && currentDur != lastDur) changingDur = ON;
                lastDur = currentDur;
            }
			currentPos++;
		}
        
        iter++;
        if (iter == beam->m_list.end()) {
            break;
        }
        current = dynamic_cast<LayerElement*>(*iter);
		if (current == NULL) {
            LogDebug("Error accessing element in Beam list");
            return;
        }
        
	}	while (1);


	previousPos = currentPos - 1;
    
    /******************************************************************/
    // Calculate the extrem values
    
    yExtremes yVals;
    int curY;
    // currentPos holds the last one
	for (i = 0; i < currentPos; i++) {
        beamElementCoord[i].y = beamElementCoord[i].element->GetDrawingY();

        // highest and lowest value;
		high= std::max(beamElementCoord[i].y, high);
		low = std::min(beamElementCoord[i].y, low);

        if (dynamic_cast<Chord*>(beamElementCoord[i].element)) {
            yVals = dynamic_cast<Chord*>(beamElementCoord[i].element)->GetYExtremes(verticalCenter);
            beamElementCoord[i].yMax = yVals.yMax;
            beamElementCoord[i].yMin = yVals.yMin;
            
            avgY += beamElementCoord[i].y + ((yVals.yMax - yVals.yMin) / 2);
            
            if (abs(yVals.yMax - verticalCenter) > abs(yExtreme - verticalCenter)) yExtreme = yVals.yMax;
            if (abs(yVals.yMin - verticalCenter) > abs(yExtreme - verticalCenter)) yExtreme = yVals.yMin;
        }
        else {
            beamElementCoord[i].y = beamElementCoord[i].element->GetDrawingY();
            curY = beamElementCoord[i].y;
            if (yExtreme >= verticalCenter && curY > yExtreme) yExtreme = curY;
            if (yExtreme <= verticalCenter && curY < yExtreme) yExtreme = curY;
            avgY += beamElementCoord[i].y;
        }
	}

    /******************************************************************/
    // Set the stem direction
    
    stemDir = layer->GetDrawingStemDir();
    if (hasChord) {
        if (yExtreme > verticalCenter) {
            stemDir = STEMDIRECTION_down;
            for (i = 0; i < currentPos; i++) {
                beamElementCoord[i].y = beamElementCoord[i].yMax;
            }
        }
        else {
            stemDir = STEMDIRECTION_up;
            for (i = 0; i < currentPos; i++) {
                beamElementCoord[i].y = beamElementCoord[i].yMin;
            }
        }
    }
    
    avgY /= currentPos;
    
    if (stemDir == STEMDIRECTION_NONE) {
        if ( avgY <  verticalCenter ) stemDir = STEMDIRECTION_up;
        else stemDir = STEMDIRECTION_down;
    }

    if (beamElementCoord[previousPos].element->m_cueSize == false)  {
        beamWidthBlack = m_doc->m_drawingBeamWidth[staff->staffSize];
        beamWidthWhite = m_doc->m_drawingBeamWhiteWidth[staff->staffSize];
    }
    else {
        beamWidthBlack = std::max(2, (m_doc->m_drawingBeamWidth[staff->staffSize] * m_doc->m_env.m_graceNum / m_doc->m_env.m_graceDen));
        beamWidthWhite = std::max(2, (m_doc->m_drawingBeamWhiteWidth[staff->staffSize] * m_doc->m_env.m_graceNum / m_doc->m_env.m_graceDen));
    }
	beamWidth = beamWidthBlack + beamWidthWhite;

    /******************************************************************/
    // Calculate the slope doing a linear regression
	
    height = 0;
    slope = 0.0;
    
    // The vertical shift depends on the shortestDur value we have in the beam
    verticalShift = ((shortestDur-DUR_8)*(beamWidth));

    if (beamElementCoord[previousPos].element->m_cueSize) {
        verticalShift += m_doc->m_drawingHalfInterl[staff->staffSize]*5;
    }
    else {
        verticalShift += (shortestDur > DUR_8) ?
            m_doc->m_drawingInterl[staff->staffSize] * verticalShiftFactor :
            m_doc->m_drawingInterl[staff->staffSize] * (verticalShiftFactor + 0.5);
    }

    // swap x position and verticalShift direction with stem down
    if (stemDir == STEMDIRECTION_down) {
        dx[0] = -dx[0];
        dx[1] = -dx[1];
        verticalShift = -verticalShift;
    }

    avgY += verticalShift;
    if ((stemDir == STEMDIRECTION_up && avgY < verticalCenter) || (stemDir == STEMDIRECTION_down && avgY > verticalCenter)) {
        verticalShift += verticalCenter - avgY;
    }

    for (i=0; i<currentPos; i++)
    {
        //change the stem dir for all objects
        if ( beamElementCoord[i].element->IsNote() ) {
            ((Note*)beamElementCoord[i].element)->m_drawingStemDir = stemDir;
            beamElementCoord[i].yBeam = beamElementCoord[i].y + verticalShift;
        }
        
        else if ( beamElementCoord[i].element->IsChord() ) {
            ((Chord*)beamElementCoord[i].element)->m_drawingStemDir = stemDir;
            beamElementCoord[i].yBeam = (stemDir == STEMDIRECTION_down ? beamElementCoord[i].yMin : beamElementCoord[i].yMax) + verticalShift;
        }
        
        beamElementCoord[i].x +=  dx[beamElementCoord[i].element->m_cueSize];
        
        s_y += beamElementCoord[i].yBeam;
        s_y2 += beamElementCoord[i].yBeam * beamElementCoord[i].yBeam;
        s_x += beamElementCoord[i].x;
        s_x2 += beamElementCoord[i].x * beamElementCoord[i].x;
        s_xy += beamElementCoord[i].x * beamElementCoord[i].yBeam;
    }


	y1 = currentPos * s_xy - s_x * s_y;
	xr = currentPos * s_x2 - s_x * s_x;

    // Prevent division by 0
    if (y1 && xr) {
		dB = y1 / xr;
    }
    else {
		dB = 0.0;
    }
    
	/* Correction esthetique : */
	if (fabs(dB) < m_doc->m_drawingBeamMinSlope ) dB = 0.0;
	if (fabs(dB) > m_doc->m_drawingBeamMaxSlope ) dB = (dB>0) ? m_doc->m_drawingBeamMaxSlope : - m_doc->m_drawingBeamMaxSlope;
	/* pente correcte: entre 0 et env 0.4 (0.2 a 0.4) */

    if (slope) {
        dB += slope;
    }
    
	dA = (s_y - dB * s_x) / currentPos;
    
    if (height) {
        dA += height;
    }

    /******************************************************************/
    // Calculate the stem lengths and draw them
    
	for ( i=0; i<currentPos; i++ ) {
        xr = beamElementCoord[i].yBeam;	/* xr, variable de travail */
		beamElementCoord[i].yBeam = dA + sy_up + dB * beamElementCoord[i].x;
		
        // LP: No idea what this does
		/* test pour garantir l'absence de recoupement */
        if (!height)
            if ((stemDir == STEMDIRECTION_up && xr > beamElementCoord[i].yBeam) || (stemDir == STEMDIRECTION_down && xr < beamElementCoord[i].yBeam)) {
                sy_up += xr - beamElementCoord[i].yBeam;
                i = -1;	/* on refait la boucle avec un sy_up */
            }
	}
    
	for (i=0; i<currentPos; i++)
	{
        if (stemDir == STEMDIRECTION_up) {
            fy1 = beamElementCoord[i].yBeam - m_doc->m_env.m_stemWidth;
            fy2 = beamElementCoord[i].y + m_doc->m_drawingHalfInterl[staff->staffSize]/4;
            beamElementCoord[i].element->m_drawingStemStart.x = beamElementCoord[i].element->m_drawingStemEnd.x = beamElementCoord[i].x;
            beamElementCoord[i].element->m_drawingStemStart.y = fy2;
            beamElementCoord[i].element->m_drawingStemEnd.y = fy1;
            beamElementCoord[i].element->m_drawingStemDir = true;
        }
        else {
            fy1 = beamElementCoord[i].yBeam + m_doc->m_env.m_stemWidth;
            fy2 = beamElementCoord[i].y - m_doc->m_drawingHalfInterl[staff->staffSize]/4;
            beamElementCoord[i].element->m_drawingStemStart.x = beamElementCoord[i].element->m_drawingStemEnd.x = beamElementCoord[i].x;
            beamElementCoord[i].element->m_drawingStemStart.y = fy2;
            beamElementCoord[i].element->m_drawingStemEnd.y = fy1;
            beamElementCoord[i].element->m_drawingStemDir = false;
        }
        if (beamElementCoord[i].element->IsNote() || beamElementCoord[i].element->IsChord()) {
            DrawVerticalLine (dc,fy2, fy1, beamElementCoord[i].x, m_doc->m_env.m_stemWidth);
		}

	}

    /******************************************************************/
    // Draw the beam full bars
    
    // Number of bars to draw - if we do not have changing values, draw
    // the number of bars according to the shortestDur value. Otherwise draw
    // only one bar and the others will be drawn separately.
    fullBars =  !changingDur ? (shortestDur - DUR_4) : 1;
    
    // Adjust the x position of the first and last element for taking into account the stem width
	beamElementCoord[0].x -= (m_doc->m_env.m_stemWidth) / 2;
	beamElementCoord[previousPos].x += (m_doc->m_env.m_stemWidth) / 2;

    // Shift direction
	shiftY = (stemDir == STEMDIRECTION_down) ? 1.0 : -1.0;

	fy1 = beamElementCoord[0].yBeam;
    fy2 = beamElementCoord[previousPos].yBeam;

	fx1 = (*beamElementCoord).x;
    fx2 = (beamElementCoord+previousPos)->x;

	s_y = shiftY;
	s_y2 = shiftY;
    
    // For acc and rit beam (see AttBeamingVis set
    // s_y = 0 and s_y2 = 0 respectively

	for (j=0; j<fullBars ; j++)
	{
		polygonHeight = beamWidthBlack*shiftY;
        DrawObliquePolygon (dc,fx1,fy1,fx2,fy2, polygonHeight);
		fy1 += polygonHeight; fy2 += polygonHeight;

        // s_y must == 0 for accelerando beams
		if (!s_y) fy1 += (beamWidthBlack * shiftY) * -1;
		else fy1 += s_y*beamWidthWhite;
        
        // reverse for retardendo beam
		if (!s_y2) fy2 += (beamWidthBlack * shiftY) * -1;
		else fy2 += s_y2*beamWidthWhite;
	}
    
    /******************************************************************/
    // Draw the beam partial bars (if any)

	/* calcul des x en cas de beaming multiple */
	/* parcours horizontal ajoutant barres en fonction de m_dur la plus 
	breve (shortestDur), controle par boucle while; la premiere boucle for
	controle le nombre d'etapes horizontales du parcours (par le nombre
	de commandes '+' enregistrees); la deuxieme boucle for teste pour
	chaque paquet entre deux '+' les valeurs; elle construit une array de 
	marqueurs partitionnant les sous-groupes; la troisieme boucle for est
	pilotee par l'indice de l'array; elle dessine horizontalement les barres 
	de chaque sous-groupe en suivant les marqueurs */

    if (changingDur) {
        testDur = DUR_8 + fullBars;
        barY = beamWidth;

        if (stemDir == STEMDIRECTION_up) {
            barY = -barY;
        }

        // loop
        while (testDur <= shortestDur) {
            t = 0; si = 0;
            i = 0;
            //for (i=0; i<=cpte_stop; i++)	/* 1e boucle for */
            {
                //fullBars = (*(st_rl+si) ? *(st_rl+si) : currentPos);
                fullBars = currentPos;
                
                /* var. test controlant 2e boucle for suivante
                 * en fonction du compteur de "+" */
    /*ici, t=j, i.e. increment des pos. de notes. On s'occupe maintenant de
    l'ensemble des valeurs contenues dans un groupe marque par cpte_stop. C'est
    la qu'il faut changer les signes des valeurs delta d'increment vertical.*/

                for(k=0; k<MAX_MIF; k++)	/* initialisation*/
                {	mx_i[k]=my_i[k]=mx_f[k]=my_f[k]=0;}

                for (j=t,_mif=0,m_i[_mif]=0; j < fullBars; j++)	/* 2e boucle. */
                {	/* j<fullBars : st_rl est trop loin de un cran */

                /* Ici, d�cision si SILENCE doit ou non avoir des barres; si oui, ligne
                    suivante (condition: il doit etre pris aussi dans les beamElementCoord plus haut):*/
                    if (((beamElementCoord+j)->dur) >= (unsigned int)testDur)	
                /*	si NON, alors: */
                    // if (((beamElementCoord+j)->dur) >= testDur && (beamElementCoord+j)->element->sil == _NOT)	
                    {
                        /*place marqueurs pour m_dur.egales/superieures
                         * a testDur en cours */
                        mx_f[_mif] = beamElementCoord[j].x;
                        my_f[_mif] = beamElementCoord[j].yBeam;
                        if(!mx_i[_mif])
                        {	mx_i[_mif] = beamElementCoord[j].x;
                            my_i[_mif] = beamElementCoord[j].yBeam;
                            if (!_mif) apax = j;
                            //if (!beamElementCoord[j].prov)
                            /* enregistre les cas ou delta y est neg.*/
                                m_i[_mif] = 1;
                        }
                    }
                    /* rupture de chaine: on passe a un 2e groupe
                     * de marqueurs */
                    else if(mx_i[_mif])
                    {	_mif++;	/*incr. s'il y a un marq.*/
                        m_i[_mif] = 0;
                    }
                }

                fbarre_y = barY;	/* stockage */
                for (k=0; k<=std::min((mx_f[_mif]?_mif:(_mif-1)),MAX_MIF); k++)
                {

                    /* on passe en revue, horizontalement, les marqueurs
                     * enregistres pour ce  groupe, en s'assurant que le
                     * max MAX_MIF n'est pas depasse */
                    if (mx_i[k] == mx_f[k])		/* une seule position concernee */
                    {
                        if (apax == t && k==0 && mx_i[k] != beamElementCoord[previousPos].x)	/* au debut du paquet */
                        {	fy1 = my_i[k] + barY;
                            mx_f[k] = mx_i[k] + m_doc->m_drawingLedgerLine[staff->staffSize][0];
                            fy2 = dA + sy_up + barY + dB * mx_f[k];

                            polygonHeight= beamWidthBlack*shiftY;
                            DrawObliquePolygon (dc,mx_i[k],fy1,mx_f[k],fy2, polygonHeight);
                            fy1 += polygonHeight; fy2 += polygonHeight;

                        }
                        else		/* corps ou fin de paquet */
                        {	fy2 = my_i[k] + barY;
                            mx_i[k] -= m_doc->m_drawingLedgerLine[staff->staffSize][0];
                            fy1 = dA + sy_up + barY + dB * mx_i[k];
                            polygonHeight = beamWidthBlack*shiftY;
                            DrawObliquePolygon (dc,mx_i[k],fy1,mx_f[k],fy2, polygonHeight);
                            fy1 += polygonHeight; fy2 += polygonHeight;

                        }
                    }
                    else if (mx_i[k])		/* s'il y a un marqueur */
                    {	fy1 = my_i[k] + barY;
                        fy2 = my_f[k] + barY;
                        polygonHeight = beamWidthBlack*shiftY;
                        DrawObliquePolygon (dc,mx_i[k],fy1,mx_f[k],fy2, polygonHeight);
                        fy1 += polygonHeight; fy2 += polygonHeight;

                    }				
                }	/* fin de boucle testant sous-ensembles marques _mif*/

                t = 0;
            }			/* fin de premiere boucle for */

            testDur += 1;
            barY += shiftY * beamWidth;
        } // end of while
    } // end of drawing partial bars

	return;	

}
    
} // namespace vrv
