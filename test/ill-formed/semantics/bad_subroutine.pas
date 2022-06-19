program badsubr;
procedure forwardButNoDelayed; forward;
         {^ error:missing-delayed-declaration }

function delayedButNoForward; begin end;
        {^ error:missing-forward-declaration }

function doubleDeclaration: integer; begin doubleDeclaration := 0 end;
        {^ note }
function doubleDeclaration; begin doubleDeclaration := 0 end;
        {^ error:duplicate-subroutine-declaration }

function tripleDeclaration: integer; forward;
function tripleDeclaration; begin tripleDeclaration := 0 end;
        {^ note }
function tripleDeclaration; begin tripleDeclaration := 0 end;
        {^ error:duplicate-subroutine-declaration }

procedure mismatchedDeclarationProcedure; forward;
         {^ note }
function mismatchedDeclarationProcedure; begin end;
        {^ error:mismatched-subroutine-declaration }
procedure mismatchedDeclarationProcedure; begin end;

function mismatchedDeclarationFunction: integer; forward;
        {^ note }
procedure mismatchedDeclarationFunction; begin end;
         {^ error:mismatched-subroutine-declaration }
function mismatchedDeclarationFunction; begin mismatchedDeclarationFunction := 0 end;

function disallowedResultType: text; begin disallowedResultType := 0 end;
                              {^ error:disallowed-result-type }
begin end.
