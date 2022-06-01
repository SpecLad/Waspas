program badtype;
const
    notType = 1;
   {^ note }
type
    a = unknown;
       {^ error:undefined-identifier }

    wrongIdTypeBuiltin = maxint;
                        {^ error:wrong-identifier-type }
    wrongIdType = notType;
                 {^ error:wrong-identifier-type }

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
                        {^ error:non-ordinal-constant }
    subrangeMismatch = 11..true;
                          {^ error:type-mismatch }
    subrangeInverted = 11..10;
                      {^ error:inverted-subrange-bounds }
var
    laterVar: (laterEnumVar);
   {^ note }  {^ note }
procedure laterProcedure; begin end;
         {^ note }
function laterFunction: integer; begin laterFunction := 0 end;
        {^ note }
begin
end.
