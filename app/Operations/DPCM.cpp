/*
 * Copyright 2011-2012 INSA Rennes
 *
 * This file is part of ImageINSA.
 *
 * ImageINSA is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ImageINSA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ImageINSA.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "DPCM.h"
#include <cstring>
#include <cstdio>
#include <cmath>
#include "../Tools.h"

using namespace std;
using namespace imagein;

DPCM::DPCM()
{
    long counter;
    for( counter=0; counter< 128; counter++ ) {
        iloiqu[counter] = 0;
    }
    for( counter=0; counter< 2048*20; counter++ ) {
        ((int*)itcod)[counter] = 0;
        ((int*)itrco)[counter] = 0;
    }
    for( counter=0; counter< 40; counter++ ) {
        ((int*)ktab)[counter] = 0;
    }
    quantdef = NULL;
}

DPCM::~DPCM()
{

}

string DPCM::execute( const GrayscaleImage *im, Prediction prediction_alg, imagein::ImageDouble **quant_err_image, imagein::ImageDouble **err_image, Image **recons_image, Image **pred_image,ImageDouble **coding_err_image, double Q ) {
    char buffer[255], buffer2[255];
    if( quantdef == NULL ) {
        throw "Error in DPCM::execute:\nquantdef = NULL";
    }
    string returnval;
    int imgHeight,imgWidth;
    int pred;
    int pred_err;   // prediction error
    int quant_pred_err;  // quantized prediction error
    int reco; // reconstructed value
    int code; // code
    int coding_err; // erreur de codage

    double pi[512],piq[512],nbpt = 0;
    double h = 0.;
    double hq = 0.;

    imgHeight = im->getHeight();
    imgWidth = im->getWidth();

    /* initialisation de la loi de quantification */
    set_levels();
    codlq(0);

    /* allocation mmoire pour l'image d'erreur de prdiction */
    ImageDouble *quantized_prediction_error_image = new ImageDouble(imgWidth, imgHeight, 1);  // renommer quantized_prediction_error_image
    ImageDouble *predction_error_image = new ImageDouble(imgWidth, imgHeight, 1);  // renommer predction_error_image
    ImageDouble *coding_error_image = new ImageDouble(imgWidth, imgHeight, 1);
    Image *reconstructed_image = new GrayscaleImage(*im);
    Image *prediction_image = new GrayscaleImage(*im);

    // Init the error images with 0 values
    for(int i=0; i < imgHeight; i++) {
        for(int j=0; j< imgWidth; j++) {
            quantized_prediction_error_image->setPixelAt(j, i, 0);
            predction_error_image->setPixelAt(j, i, 0);
        }
    }

    /* codage de l'image */
    for(int i=1; i<imgHeight ; i++)
    {
        for(int j=1; j<imgWidth ; j++)
        {
            switch (prediction_alg) {
            int AplusC;
            float A,B,C;

            case PX_EQ_A:
                pred = reconstructed_image->getPixelAt(j-1, i);
                break;
            case PX_EQ_B:
                pred = reconstructed_image->getPixelAt(j, i-1);
                break;
            case PX_EQ_APC:
                AplusC = reconstructed_image->getPixelAt(j-1, i) + reconstructed_image->getPixelAt(j, i-1);
                pred = AplusC / 2;
                break;
            case PX_EQ_Q:
                /*
                Modified Graham's Algorithm:
                if |B-C| - Q <= |B-A| <= |B-C| + Q
                        P(X) = (A+C)/2
                else
                    if |B-A| > |B-C|
                            P(X) = A
                    else
                            P(X) = C
                */
               /* A = im->getPixelAt(j-1, i);
                B = im->getPixelAt(j-1, i-1);
                C = im->getPixelAt(j,   i-1);
                */

                //correction QB
                A = reconstructed_image->getPixelAt(j-1, i);
                B = reconstructed_image->getPixelAt(j-1, i-1);
                C = reconstructed_image->getPixelAt(j,   i-1);

                if( ((fabs(B-C) - Q) <= fabs(B-A)) &&
                        (fabs(B-A) <= (fabs(B-C) + Q)) ) {
                    pred = (uint8_t)((A + C) / 2);
                } else {
                    if( fabs(B-A) > fabs(B-C) ) {
                        pred = (uint8_t)A;
                    } else {
                        pred = (uint8_t)C;
                    }
                }
                break;
            default:
                break;
            }

            //image de prediction pour affichage
            prediction_image->setPixelAt(j,i,pred);
            depth_default_t thePixel = reconstructed_image->getPixelAt(j, i);
            //erreur de prediction
            pred_err = thePixel - pred;
            predction_error_image->setPixelAt(j, i, pred_err);

            depth_default_t pixImg = im->getPixelAt(j, i);

            //code obsolete
            //codec(0, quant_pred_err, &code, &reco);//action : voir code Vero 1988 calcul : reco = quant_pred_err (erreur de prediction quantifiee)
            //int tempvalue = pred + reco;

            //quantification erreur de prediction
            quant_pred_err = quantdef->valueOf(pred_err);
            quantized_prediction_error_image->setPixelAt(j, i, quant_pred_err);


            quantized_prediction_error_image->setPixelAt(j, i, quant_pred_err);

            // valeur reconstruite
            int tempvalue = pred + quant_pred_err;
            // Crop the value in [0,255]
            reconstructed_image->setPixelAt(j, i, tempvalue > 255 ? 255 : tempvalue < 0 ? 0 : tempvalue);
            depth_default_t reconstructedPix = reconstructed_image->getPixelAt(j, i);
            //calcul de l'erreur de codage
            coding_err = pixImg - reconstructedPix;
            //construction de l'image d'erreur de codage
            coding_error_image->setPixelAt(j,i,(coding_err));

        }
    }

    
    double pred_err_entrop = predction_error_image->getEntropy();
    sprintf(buffer, qApp->translate("DPCM","\nL'entropie de l'image d'erreur de prediction vaut : %f\n").toUtf8(), pred_err_entrop);


    double quant_pred_err_entrop = quantized_prediction_error_image->getEntropy();
    sprintf(buffer2, qApp->translate("DPCM", "\nL'entropie de l'image d'erreur de prediction quantifiee vaut : %f\n").toUtf8(), quant_pred_err_entrop);


    returnval = returnval + buffer;
    returnval = returnval + "\n";
    returnval = returnval + buffer2;
    returnval = returnval + "\n";
    returnval = returnval + print_iloiqu();

    /* libration de la mmoire alloue */
    *quant_err_image = quantized_prediction_error_image;
    *err_image = predction_error_image;
    *recons_image = reconstructed_image;
    *pred_image = prediction_image;
    *coding_err_image = coding_error_image;
    return returnval;
}

void DPCM::codlq(int m) {
    int n,nar,nk,i,j;

    n=iloiqu[0];
    ktab[0][m]=iloiqu[1];
    nar=ktab[0][m] - 1;
    j= -1;
    for(i=0;i < n-1 ; i++)
    {
        nk=1+nar-iloiqu[2*i+1];
        do{j++;itcod[j][m]=i;nk++;} while(nk <= 0);
        nar=iloiqu[2*i+1];
        itrco[i][m]=iloiqu[2*i+2];
    }
    itcod[j+1][m]=i;
    itrco[i][m]=iloiqu[2*i+2];
    ktab[1][m]=iloiqu[2*i+1];
}

void DPCM::codec(int nlq,int ier,int *icode,int *ireco) {
    int m,ip,iep,ierp,n,l;
    m=nlq;
    ip=ktab[0][m];
    ierp=ier-ip;
    if(ierp > 0)
    {
        ip=ktab[1][m];
        iep=ier-ip;
        if(iep < 0)
            n=ier - ktab[0][m];
        else
            n=ktab[1][m] - ktab[0][m];
        *icode=itcod[n][m];
        l = *icode;
        *ireco=itrco[l][m];
    }
    else
    {
        *icode=itcod[0][m];
        *ireco=itrco[0][m];
    }
}

void DPCM::set_levels() {
    // Fills in iloiqu with the specified values
    if( quantdef->nbThresholds() > N_MAX_THRESHOLD || quantdef->nbThresholds() < 1 ) {
        char buffer[255];
        sprintf( buffer, qApp->translate("DPCM","Error in DPCM::set_levels:\nquantdef->GetNumThresholds() = %d").toUtf8(), quantdef->nbThresholds() );
        throw buffer;
    }
    int counter;
    iloiqu[0] = quantdef->nbValues();
    for( counter=0; counter< quantdef->nbThresholds(); counter++ ) {
        iloiqu[ counter * 2 + 1 ] = quantdef->threshold(counter);
        iloiqu[ counter * 2 + 2 ] = quantdef->value(counter);
    }
    iloiqu[quantdef->nbThresholds() * 2 + 1 ] = iloiqu[quantdef->nbThresholds() * 2 - 1 ] + 1;
    iloiqu[quantdef->nbThresholds() * 2 + 2 ] = quantdef->value(quantdef->nbThresholds());
}

string DPCM::print_iloiqu() {
    string returnval;
    returnval = qApp->translate("DPCM","seuils de decision --------------- niveaux de reconstruction\n").toStdString();
    int counter;
    char buffer[100];
    for( counter=1; counter<= iloiqu[0]*2-1; counter++ ) {
        if( !(counter & 1 == 1) ) {
            sprintf( buffer, "                                                 %3d     \n", iloiqu[counter] );
            returnval = returnval + buffer;
            sprintf( buffer, "      %3d ---------------------------------------------\n", iloiqu[counter-1] );
            returnval = returnval + buffer;
        }
    }
    sprintf( buffer, "                                                 %3d     \n", iloiqu[counter] );
    returnval = returnval + buffer;

    return returnval;
}

void DPCM::setQuantification( Quantification *tquantdef ) {
    if( tquantdef == NULL ) {
        throw "Error in DPCM::setQuantDef:\ntquantdef = NULL";
    }
    if( tquantdef->nbThresholds() > N_MAX_THRESHOLD || tquantdef->nbThresholds() < 1 ) {
        char buffer[255];
        sprintf( buffer, qApp->translate("DPCM","Error in DPCM::setQuantDef:\ntquantdef->GetNumThresholds() = %d").toUtf8(), tquantdef->nbThresholds() );
        throw buffer;
    }
    quantdef = tquantdef;
}
