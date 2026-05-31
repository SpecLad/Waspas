{
SPDX-FileCopyrightText: (C) Roman Donchenko <rdonchen@outlook.com>

SPDX-License-Identifier: MPL-2.0
}

program badparams;
type
    acceptableBound = -10..10;
    tag = 0..1;

    rec = record
        case t: tag of
            0: (); 1: ();
    end;
    packedRec = packed record
        t: tag;
    end;
var
    goodArray: packed array [1..10] of tag;
    arrayWithDifferentBounds: packed array [-10..-1] of tag;
    arrayWithIncompatibleIndexType: packed array [boolean] of tag;
    arrayWithBadLowerBound: packed array [-100..10] of tag;
    arrayWithBadUpperBound: packed array [-10..100] of tag;
    arrayWithNonconformableComponentType: packed array [1..10] of char;
    unpackedArray: array [1..10] of tag;

    intVar: integer;
    tagVar: tag;
    recVar: rec;
    packedRecVar: packedRec;
function f(i: integer; procedure r): integer;
    begin f := 0 end;
procedure p(i: integer; procedure r);
    begin
        r(1);
         {^ error:parameter-count-mismatch }
    end;
function fvar(var i: tag): integer;
    begin fvar := 0 end;
procedure pvar(var i: tag);
    begin end;
function ffunc(function f(v: array [m..n: integer] of integer; procedure r): integer): integer;
    begin
        ffunc := 0;
        intVar := f;
                  {^ error:parameter-count-mismatch }
    end;
procedure pfunc(function f(v: array [m..n: integer] of integer; procedure r): integer);
    begin end;
function g(a, b: packed array[m..n: acceptableBound] of tag): integer;
    begin g := 0 end;
procedure q(a, b: packed array[m..n: acceptableBound] of tag);
    begin
        q(a, goodArray);
         {^ error:disallowed-parameter-form }

        intVar := g(a, goodArray);
                   {^ error:disallowed-parameter-form }

        pvar(a[m]);
            {^ error:disallowed-parameter-form }

        intVar := fvar(a[m]);
                      {^ error:disallowed-parameter-form }
    end;
function gvar(var a, b: packed array[m..n: acceptableBound] of tag): integer;
    begin gvar := 0 end;
procedure qvar(var a, b: packed array[m..n: acceptableBound] of tag);
    begin end;
procedure goodProc; begin end;
function goodFunc(v: array [m..n: integer] of integer; procedure r): integer;
    begin goodFunc := 0 end;
function badFuncWrongNumParams: integer;
    begin badFuncWrongNumParams := 0 end;
function badFuncVarMismatch(var v: array [m..n: integer] of integer; procedure r): integer;
    begin badFuncVarMismatch := 0 end;
function badFuncWrongNumNames(v, w: array [m..n: integer] of integer; procedure r): integer;
    begin badFuncWrongNumNames := 0 end;
function badFuncBoundTypeMismatch(v: array [m..n: acceptableBound] of integer; procedure r): integer;
    begin badFuncBoundTypeMismatch := 0 end;
function badFuncComponentTypeMismatch(v: array [m..n: integer] of real; procedure r): integer;
    begin badFuncComponentTypeMismatch := 0 end;
function badFuncPackedMismatch(v: packed array [m..n: integer] of integer; procedure r): integer;
    begin badFuncPackedMismatch := 0 end;
function badFuncSignatureMismatch(v: array [m..n: integer] of integer; procedure r(i: integer)): integer;
    begin badFuncSignatureMismatch := 0 end;
function badFuncWrongResultType(v: array [m..n: integer] of integer; procedure r): real;
    begin badFuncWrongResultType := 0 end;
begin
    intVar := f;
              {^ error:parameter-count-mismatch }
    p;
    {^ error:parameter-count-mismatch }
    intVar := f(1);
                {^ error:parameter-count-mismatch }
    p(1);
      {^ error:parameter-count-mismatch }
    intVar := f(1, goodProc, 1);
                            {^ error:parameter-count-mismatch }
    p(1, goodProc, 1);
                  {^ error:parameter-count-mismatch }

    intVar := f(2.3, goodProc);
               {^ error:type-mismatch }
    p(2.3, goodProc);
     {^ error:type-mismatch }

    intVar := f(1:10, goodProc);
                {^ error:disallowed-parameter-form }
    p(1:10, goodProc);
      {^ error:disallowed-parameter-form }

    intVar := f(1, goodFunc);
                  {^ error:wrong-identifier-kind }
    p(1, goodFunc);
        {^ error:wrong-identifier-kind }

    intVar := f(1, write);
                  {^ error:disallowed-parameter-form }
    p(1, write);
        {^ error:disallowed-parameter-form }

    intVar := f(1, sin);
                  {^ error:wrong-identifier-kind }
    p(1, sin);
        {^ error:wrong-identifier-kind }

    intVar := fvar(intVar);
                  {^ error:type-mismatch }
    pvar(intVar);
        {^ error:type-mismatch }

    intVar := fvar(tagVar = 0);
                         {^ error:disallowed-parameter-form }
    pvar(tagVar = 0);
               {^ error:disallowed-parameter-form }
    intVar := fvar(+tagVar);
                  {^ error:disallowed-parameter-form }
    pvar(+tagVar);
        {^ error:disallowed-parameter-form }
    intVar := fvar(tagVar + 1);
                         {^ error:disallowed-parameter-form }
    pvar(tagVar + 1);
               {^ error:disallowed-parameter-form }
    intVar := fvar(tagVar * 2);
                         {^ error:disallowed-parameter-form }
    pvar(tagVar * 2);
               {^ error:disallowed-parameter-form }
    intVar := fvar(1);
                  {^ error:disallowed-parameter-form }
    pvar(1);
        {^ error:disallowed-parameter-form }

    intVar := fvar(recVar.t);
                  {^ error:disallowed-parameter-form }
    pvar(recVar.t);
        {^ error:disallowed-parameter-form }

    intVar := fvar(packedRecVar.t);
                  {^ error:disallowed-parameter-form }
    pvar(packedRecVar.t);
        {^ error:disallowed-parameter-form }

    intVar := fvar(goodArray[1]);
                  {^ error:disallowed-parameter-form }
    pvar(goodArray[1]);
        {^ error:disallowed-parameter-form }

    intVar := fvar(tagVar:10);
                        {^ error:disallowed-parameter-form }
    pvar(tagVar:10);
              {^ error:disallowed-parameter-form }

    intVar := ffunc(goodFunc = 0);
                            {^ error:disallowed-parameter-form }
    pfunc(goodFunc = 0);
                  {^ error:disallowed-parameter-form }
    intVar := ffunc(+goodFunc);
                   {^ error:disallowed-parameter-form }
    pfunc(+goodFunc);
         {^ error:disallowed-parameter-form }
    intVar := ffunc(goodFunc + 1);
                            {^ error:disallowed-parameter-form }
    pfunc(goodFunc + 1);
                  {^ error:disallowed-parameter-form }
    intVar := ffunc(goodFunc * 2);
                            {^ error:disallowed-parameter-form }
    pfunc(goodFunc * 2);
                  {^ error:disallowed-parameter-form }
    intVar := ffunc(goodFunc.foobar);
                           {^ error:disallowed-parameter-form }
    pfunc(goodFunc.foobar);
                 {^ error:disallowed-parameter-form }
    intVar := ffunc(1);
                   {^ error:disallowed-parameter-form }
    pfunc(1);
         {^ error:disallowed-parameter-form }

    intVar := ffunc(goodFunc:10);
                           {^ error:disallowed-parameter-form }
    pfunc(goodFunc:10);
                 {^ error:disallowed-parameter-form }

    intVar := ffunc(undefined);
                   {^ error:undefined-identifier }
    pfunc(undefined);
         {^ error:undefined-identifier }

    intVar := ffunc(intVar);
                   {^ error:wrong-identifier-kind }
    pfunc(intVar);
         {^ error:wrong-identifier-kind }

    intVar := ffunc(goodProc);
                   {^ error:wrong-identifier-kind }
    pfunc(goodProc);
         {^ error:wrong-identifier-kind }

    intVar := ffunc(write);
                   {^ error:wrong-identifier-kind }
    pfunc(write);
         {^ error:wrong-identifier-kind }

    intVar := ffunc(sin);
                   {^ error:disallowed-parameter-form }
    pfunc(sin);
         {^ error:disallowed-parameter-form }

    intVar := ffunc(badFuncWrongNumParams);
                   {^ error:type-mismatch }
    pfunc(badFuncWrongNumParams);
         {^ error:type-mismatch }
    intVar := ffunc(badFuncVarMismatch);
                   {^ error:type-mismatch }
    pfunc(badFuncVarMismatch);
         {^ error:type-mismatch }
    intVar := ffunc(badFuncWrongNumNames);
                   {^ error:type-mismatch }
    pfunc(badFuncWrongNumNames);
         {^ error:type-mismatch }
    intVar := ffunc(badFuncBoundTypeMismatch);
                   {^ error:type-mismatch }
    pfunc(badFuncBoundTypeMismatch);
         {^ error:type-mismatch }
    intVar := ffunc(badFuncComponentTypeMismatch);
                   {^ error:type-mismatch }
    pfunc(badFuncComponentTypeMismatch);
         {^ error:type-mismatch }
    intVar := ffunc(badFuncPackedMismatch);
                   {^ error:type-mismatch }
    pfunc(badFuncPackedMismatch);
         {^ error:type-mismatch }
    intVar := ffunc(badFuncSignatureMismatch);
                   {^ error:type-mismatch }
    pfunc(badFuncSignatureMismatch);
         {^ error:type-mismatch }
    intVar := ffunc(badFuncWrongResultType);
                   {^ error:type-mismatch }
    pfunc(badFuncWrongResultType);
         {^ error:type-mismatch }

    intVar := g(goodArray, arrayWithDifferentBounds);
               {^ note }  {^ error:type-mismatch }
    q(goodArray, arrayWithDifferentBounds);
     {^ note }  {^ error:type-mismatch }

    intVar := g(arrayWithIncompatibleIndexType, goodArray);
               {^ error:type-mismatch }
    q(arrayWithIncompatibleIndexType, goodArray);
     {^ error:type-mismatch }
    intVar := g(arrayWithBadLowerBound, goodArray);
               {^ error:type-mismatch }
    q(arrayWithBadLowerBound, goodArray);
     {^ error:type-mismatch }
    intVar := g(arrayWithBadUpperBound, goodArray);
               {^ error:type-mismatch }
    q(arrayWithBadUpperBound, goodArray);
     {^ error:type-mismatch }
    intVar := g(arrayWithNonconformableComponentType, goodArray);
               {^ error:type-mismatch }
    q(arrayWithNonconformableComponentType, goodArray);
     {^ error:type-mismatch }
    intVar := g(unpackedArray, goodArray);
               {^ error:type-mismatch }
    q(unpackedArray, goodArray);
     {^ error:type-mismatch }

    intVar := gvar(goodArray, arrayWithDifferentBounds);
                  {^ note }  {^ error:type-mismatch }
    qvar(goodArray, arrayWithDifferentBounds);
        {^ note }  {^ error:type-mismatch }

    intVar := gvar(arrayWithIncompatibleIndexType, goodArray);
                  {^ error:type-mismatch }
    qvar(arrayWithIncompatibleIndexType, goodArray);
        {^ error:type-mismatch }
    intVar := gvar(arrayWithBadLowerBound, goodArray);
                  {^ error:type-mismatch }
    qvar(arrayWithBadLowerBound, goodArray);
        {^ error:type-mismatch }
    intVar := gvar(arrayWithBadUpperBound, goodArray);
                  {^ error:type-mismatch }
    qvar(arrayWithBadUpperBound, goodArray);
        {^ error:type-mismatch }
    intVar := gvar(arrayWithNonconformableComponentType, goodArray);
                  {^ error:type-mismatch }
    qvar(arrayWithNonconformableComponentType, goodArray);
        {^ error:type-mismatch }
    intVar := gvar(unpackedArray, goodArray);
                  {^ error:type-mismatch }
    qvar(unpackedArray, goodArray);
        {^ error:type-mismatch }
end.
