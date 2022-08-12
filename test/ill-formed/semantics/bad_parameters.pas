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
procedure q(a, b: packed array[m..n: acceptableBound] of tag);
    begin
        q(a, goodArray);
         {^ error:disallowed-parameter-form }

        pvar(a[m]);
            {^ error:disallowed-parameter-form }
    end;
procedure qvar(var a, b: packed array[m..n: acceptableBound] of tag);
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
