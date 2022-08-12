program badparams;
type
    acceptableBound = -10..10;
var
    goodArray: array [1..10] of integer;
    arrayWithDifferentBounds: array [-10..-1] of integer;
    arrayWithIncompatibleIndexType: array [boolean] of integer;
    arrayWithBadLowerBound: array [-100..10] of integer;
    arrayWithBadUpperBound: array [-10..100] of integer;
    arrayWithNonconformableComponentType: array [1..10] of char;
    packedArray: packed array [1..10] of integer;

    intVar: integer;
    realVar: real;
procedure p(i: integer; procedure r);
    begin end;
procedure pvar(var r: real);
    begin end;
procedure q(a, b: array[m..n: acceptableBound] of integer);
    begin
        q(a, goodArray);
         {^ error:disallowed-parameter-form }
    end;
procedure qvar(var a, b: array[m..n: acceptableBound] of integer);
    begin end;
procedure r; begin end;

begin
    p;
    {^ error:parameter-count-mismatch }
    p(1);
      {^ error:parameter-count-mismatch }
    p(1, r, 1);
           {^ error:parameter-count-mismatch }

    p(2.3, r);
     {^ error:type-mismatch }

    p(1:10, r);
      {^ error:disallowed-parameter-form }

    pvar(intVar);
        {^ error:type-mismatch }

    pvar(realVar = 0);
                {^ error:disallowed-parameter-form }
    pvar(+realVar);
        {^ error:disallowed-parameter-form }
    pvar(realVar + 1);
                {^ error:disallowed-parameter-form }
    pvar(realVar * 2);
                {^ error:disallowed-parameter-form }
    pvar(1.1);
        {^ error:disallowed-parameter-form }

    pvar(realVar:10:3);
               {^ error:disallowed-parameter-form }

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
    q(packedArray, goodArray);
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
    qvar(packedArray, goodArray);
        {^ error:type-mismatch }
end.
