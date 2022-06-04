program badtype;
const
    notType = 1;
   {^ note }
    notType2 = 2;
   {^ note }
type
    a = unknown;
       {^ error:undefined-identifier }

    wrongIdKindBuiltin = maxint;
                        {^ error:wrong-identifier-kind }
    wrongIdKind = notType;
                 {^ error:wrong-identifier-kind }

    selfRef = selfRef;
             {^ error:circular-definition }

    { various variations on use-before-definition }
    ubdType = laterType;
             {^ error:use-before-definition }
    ubdEnum = laterEnum;
             {^ error:use-before-definition }
    laterType = (laterEnum);
   {^ note }    {^ note }
    ubdVar = laterVar;
            {^ error:use-before-definition }
    ubdEnumVar = laterEnumVar;
                {^ error:use-before-definition }
    ubdProcedure = laterProcedure;
                  {^ error:use-before-definition }
    ubdFunction = laterFunction;
                 {^ error:use-before-definition }

    subrangeNonOrdinal = 'abc'..'def';
                        {^ error:non-ordinal-type }
    subrangeMismatch = 11..true;
                          {^ error:type-mismatch }
    subrangeInverted = 11..10;
                          {^ error:inverted-subrange-bounds }

    arrayNonOrdinalIndex = array[real] of integer;
                                {^ error:non-ordinal-type }

    fileOfFile = file of file of integer;
                        {^ error:disallowed-file-component }
    fileOfText = file of text;
                        {^ error:disallowed-file-component }
    fileOfFileArray = file of array[1..10] of file of integer;
                             {^ error:disallowed-file-component }

    setOfNonOrdinal = set of real;
                            {^ error:non-ordinal-type }

    pointerToUndefined = ^undefined;
                         {^ error:undefined-identifier }
    pointerToNonTypeBuiltin = ^maxint;
                              {^ error:wrong-identifier-kind }
    pointerToNonType = ^notType2;
                       {^ error:wrong-identifier-kind }

    recordWithNonOrdinalTag = record
        case real of 1: ();
            {^ error:non-ordinal-type }
    end;

    recordWithNonOrdinalCaseConstant = record
        case boolean of 1.1: ();
                       {^ error:non-ordinal-type }
    end;
var
    laterVar: (laterEnumVar);
   {^ note }  {^ note }
procedure laterProcedure; begin end;
         {^ note }
function laterFunction: integer; begin laterFunction := 0 end;
        {^ note }
begin
end.
