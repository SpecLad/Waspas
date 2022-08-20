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
procedure p(i: integer; procedure r);
    begin end;
procedure pvar(var i: tag);
    begin end;
procedure pfunc(function f(v: array [m..n: integer] of integer; procedure r): integer);
    begin end;
procedure q(a, b: packed array[m..n: acceptableBound] of tag);
    begin
        q(a, goodArray);
         {^ error:disallowed-parameter-form }

        pvar(a[m]);
            {^ error:disallowed-parameter-form }
    end;
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
    p;
    {^ error:parameter-count-mismatch }
    p(1);
      {^ error:parameter-count-mismatch }
    p(1, goodProc, 1);
                  {^ error:parameter-count-mismatch }

    p(2.3, goodProc);
     {^ error:type-mismatch }

    p(1:10, goodProc);
      {^ error:disallowed-parameter-form }

    p(1, goodFunc);
        {^ error:wrong-identifier-kind }

    pvar(intVar);
        {^ error:type-mismatch }

    pvar(tagVar = 0);
               {^ error:disallowed-parameter-form }
    pvar(+tagVar);
        {^ error:disallowed-parameter-form }
    pvar(tagVar + 1);
               {^ error:disallowed-parameter-form }
    pvar(tagVar * 2);
               {^ error:disallowed-parameter-form }
    pvar(1);
        {^ error:disallowed-parameter-form }

    pvar(recVar.t);
        {^ error:disallowed-parameter-form }

    pvar(packedRecVar.t);
        {^ error:disallowed-parameter-form }

    pvar(goodArray[1]);
        {^ error:disallowed-parameter-form }

    pvar(tagVar:10);
              {^ error:disallowed-parameter-form }

    pfunc(goodFunc = 0);
                  {^ error:disallowed-parameter-form }
    pfunc(+goodFunc);
         {^ error:disallowed-parameter-form }
    pfunc(goodFunc + 1);
                  {^ error:disallowed-parameter-form }
    pfunc(goodFunc * 2);
                  {^ error:disallowed-parameter-form }
    pfunc(goodFunc.foobar);
                 {^ error:disallowed-parameter-form }
    pfunc(1);
         {^ error:disallowed-parameter-form }

    pfunc(goodFunc:10);
                 {^ error:disallowed-parameter-form }

    pfunc(undefined);
         {^ error:undefined-identifier }

    pfunc(intVar);
         {^ error:wrong-identifier-kind }

    pfunc(goodProc);
         {^ error:wrong-identifier-kind }

    pfunc(badFuncWrongNumParams);
         {^ error:type-mismatch }
    pfunc(badFuncVarMismatch);
         {^ error:type-mismatch }
    pfunc(badFuncWrongNumNames);
         {^ error:type-mismatch }
    pfunc(badFuncBoundTypeMismatch);
         {^ error:type-mismatch }
    pfunc(badFuncComponentTypeMismatch);
         {^ error:type-mismatch }
    pfunc(badFuncPackedMismatch);
         {^ error:type-mismatch }
    pfunc(badFuncSignatureMismatch);
         {^ error:type-mismatch }
    pfunc(badFuncWrongResultType);
         {^ error:type-mismatch }

    q(goodArray, arrayWithDifferentBounds);
     {^ note }  {^ error:type-mismatch }

    q(arrayWithIncompatibleIndexType, goodArray);
     {^ error:type-mismatch }
    q(arrayWithBadLowerBound, goodArray);
     {^ error:type-mismatch }
    q(arrayWithBadUpperBound, goodArray);
     {^ error:type-mismatch }
    q(arrayWithNonconformableComponentType, goodArray);
     {^ error:type-mismatch }
    q(unpackedArray, goodArray);
     {^ error:type-mismatch }

    qvar(goodArray, arrayWithDifferentBounds);
        {^ note }  {^ error:type-mismatch }

    qvar(arrayWithIncompatibleIndexType, goodArray);
        {^ error:type-mismatch }
    qvar(arrayWithBadLowerBound, goodArray);
        {^ error:type-mismatch }
    qvar(arrayWithBadUpperBound, goodArray);
        {^ error:type-mismatch }
    qvar(arrayWithNonconformableComponentType, goodArray);
        {^ error:type-mismatch }
    qvar(unpackedArray, goodArray);
        {^ error:type-mismatch }
end.
