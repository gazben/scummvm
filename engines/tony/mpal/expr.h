/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *
 */
/**************************************************************************
 *                                     ���������������������������������� *
 *                        ...                  Spyral Software snc        *
 *        .             x#""*$Nu       -= We create much MORE than ALL =- *
 *      d*#R$.          R     ^#$o     ���������������������������������� *
 *    .F    ^$k         $        "$b                                      *
 *   ."       $b      u "$         #$L                                    *
 *   P         $c    :*$L"$L        '$k  Project: MPAL................... *
 *  d    @$N.   $.   d ^$b^$k         $c                                  *
 *  F   4  "$c  '$   $   #$u#$u       '$ Module:  Expression gestor heade *
 * 4    4k   *N  #b .>    '$N'*$u      *                                  *
 * M     $L   #$  $ 8       "$c'#$b.. .@ Author:  Giovanni Bajo.......... *
 * M     '$u   "$u :"         *$. "#*#"                                   *
 * M      '$N.  "  F           ^$k       Desc:    Gestisce le espressioni *
 * 4>       ^R$oue#             d                 matematiche............ *
 * '$          ""              @                  ....................... *
 *  #b                       u#                                           *
 *   $b                    .@"           OS: [ ] DOS  [X] WIN95  [ ] OS/2 *
 *    #$u                .d"                                              *
 *     '*$e.          .zR".@           ���������������������������������� *
 *        "*$$beooee$*"  @"M                  This source code is         *
 *             """      '$.?              Copyright (C) Spyral Software   *
 *                       '$d>                 ALL RIGHTS RESERVED         *
 *                        '$>          ���������������������������������� *
 *                                                                        *
 **************************************************************************/

#ifndef MPAL_EXPR_H
#define MPAL_EXPR_H

namespace Tony {

namespace MPAL {

/****************************************************************************\
*       Function Prototypes
\****************************************************************************/

/**
 * Parses a mathematical expression from the MPC file
 *
 * @param buf				Buffer containing the expression to evaluate
 * @param h					Pointer to a handle that, at the end of execution, 
 * will point to the area of memory containing the parsed expression
 * @returns		Pointer to the buffer immediately after the expression, or NULL if error.
 */
const byte *ParseExpression(const byte *lpBuf, HGLOBAL *h);


/**
 * Calculate the value of a mathamatical expression
 *
 * @param h					Handle to the expression
 * @returns		Numeric value
 */
int EvaluateExpression(HGLOBAL h);


/**
 * Compare two mathematical expressions together
 *
 * @param h1				Expression to be compared
 * @param h2				Expression to be compared
 */
bool CompareExpressions(HGLOBAL h1, HGLOBAL h2);

} // end of namespace MPAL

} // end of namespace Tony

#endif