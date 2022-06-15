program badsubr;
procedure forwardButNoDelayed; forward;
         {^ error:missing-delayed-declaration }

function delayedButNoForward; begin end;
        {^ error:missing-forward-declaration }

function doubleDeclaration: integer; begin doubleDeclaration := 0 end;
function doubleDeclaration; begin doubleDeclaration := 0 end;
        {^ error:duplicate-subroutine-declaration }

function tripleDeclaration: integer; forward;
function tripleDeclaration; begin tripleDeclaration := 0 end;
function tripleDeclaration; begin tripleDeclaration := 0 end;
        {^ error:duplicate-subroutine-declaration }
begin end.
