program bpb(input, output);
type
    tagType = 0..1;
    recType = record
        case tagType of 0: (); 1: ();
    end;
var
    i: integer;
    pi: ^integer;
    fi: file of integer;
    t: text;

    pr: ^recType;
begin
    { dispose }

    dispose;
          {^ error:parameter-count-mismatch }

    dispose(i);
           {^ error:type-mismatch }

    dispose(pi:3);
             {^ error:disallowed-parameter-form }

    dispose(pi, 1);
               {^ error:parameter-count-mismatch }

    dispose(pr, 1.1);
               {^ error:type-mismatch }

    dispose(pr, 2);
               {^ error:out-of-range }

    dispose(pr, 1:3);
                {^ error:disallowed-parameter-form }

    dispose(pr, 1, 'a');
                  {^ error:parameter-count-mismatch }

    { get }

    get;
      {^ error:parameter-count-mismatch }
    get(i);
       {^ error:type-mismatch }
    get(fi:3);
         {^ error:disallowed-parameter-form }
    get(fi, 1);
           {^ error:parameter-count-mismatch }

    { new }

    new;
      {^ error:parameter-count-mismatch }

    new(i);
       {^ error:type-mismatch }

    new(pi:3);
         {^ error:disallowed-parameter-form }

    new(pi, 1);
           {^ error:parameter-count-mismatch }

    new(pr, 1.1);
           {^ error:type-mismatch }

    new(pr, 2);
           {^ error:out-of-range }

    new(pr, 1:3);
            {^ error:disallowed-parameter-form }

    new(pr, 1, 'a');
              {^ error:parameter-count-mismatch }

    { new with invalid constants }

    new(pr, 1 = 0);
             {^ error:disallowed-parameter-form }
    new(pr, 1 + 1);
             {^ error:disallowed-parameter-form }
    new(pr, 1 * 1);
             {^ error:disallowed-parameter-form }
    new(pr, maxint.foobar);
                 {^ error:disallowed-parameter-form }
    new(pr, (1));
           {^ error:disallowed-parameter-form }

    { read (???) }

    read;
       {^ error:parameter-count-mismatch }

    { read (typed) }

    read(fi);
          {^ error:parameter-count-mismatch }

    read(fi:3, i);
          {^ error:disallowed-parameter-form }

    read(fi, pi);
            {^ error:type-mismatch }

    read(fi, i:3);
             {^ error:disallowed-parameter-form }

    { read (text) }

    read(t);
         {^ error:parameter-count-mismatch }

    read(t:3, i);
         {^ error:disallowed-parameter-form }

    read(t, t);
           {^ error:type-mismatch }

    read(t, i:3);
            {^ error:disallowed-parameter-form }

    read(pi);
        {^ error:type-mismatch }

    read(i:3);
         {^ error:disallowed-parameter-form }

    { readln }

    readln(t:3);
           {^ error:disallowed-parameter-form }

    readln(t, t);
             {^ error:type-mismatch }

    readln(t, i:3);
              {^ error:disallowed-parameter-form }

    readln(fi);
          {^ error:type-mismatch }

    readln(i:3);
           {^ error:disallowed-parameter-form }

    { reset }

    reset;
        {^ error:parameter-count-mismatch }
    reset(i);
         {^ error:type-mismatch }
    reset(fi:3);
           {^ error:disallowed-parameter-form }
    reset(fi, 1);
             {^ error:parameter-count-mismatch }

    { rewrite }

    rewrite;
          {^ error:parameter-count-mismatch }
    rewrite(i);
           {^ error:type-mismatch }
    rewrite(fi:3);
             {^ error:disallowed-parameter-form }
    rewrite(fi, 1);
               {^ error:parameter-count-mismatch }

    { page }
    page(fi);
        {^ error:type-mismatch }
    page(t:3);
         {^ error:disallowed-parameter-form }
    page(t, 1);
           {^ error:parameter-count-mismatch }

    { put }

    put;
      {^ error:parameter-count-mismatch }
    put(i);
       {^ error:type-mismatch }
    put(fi:3);
         {^ error:disallowed-parameter-form }
    put(fi, 1);
           {^ error:parameter-count-mismatch }

    { write (???) }

    write;
        {^ error:parameter-count-mismatch }

    { write (typed) }

    write(fi);
           {^ error:parameter-count-mismatch }

    write(fi.x, 1);
           {^ error:non-record-type } { testing that the error is only emitted once }

    write(fi:3, 1);
           {^ error:disallowed-parameter-form }

    write(fi, 1:3);
              {^ error:disallowed-parameter-form }

    write(fi, 1.1);
             {^ error:type-mismatch }

    { write (text) }

    write(t);
          {^ error:parameter-count-mismatch }

    write(t.x, 1);
          {^ error:non-record-type } { testing that the error is only emitted once }

    write(t:3, 1);
          {^ error:disallowed-parameter-form }

    write(t, t);
            {^ error:type-mismatch }

    write(t, 1:1.1);
              {^ error:type-mismatch }

    write(t, 1:1:1.1);
                {^ error:disallowed-parameter-form }

    write(t, 1.1:1:1.1);
                  {^ error:type-mismatch }

    write(pi);
         {^ error:type-mismatch }

    write(1:1.1);
           {^ error:type-mismatch }

    write(1:1:1.1);
             {^ error:disallowed-parameter-form }

    write(1.1:1:1.1);
               {^ error:type-mismatch }

    { writeln }

    writeln(t.x);
            {^ error:non-record-type } { testing that the error is only emitted once }

    writeln(t:3);
            {^ error:disallowed-parameter-form }

    writeln(t, t);
              {^ error:type-mismatch }

    writeln(t, 1:1.1);
                {^ error:type-mismatch }

    writeln(t, 1:1:1.1);
                  {^ error:disallowed-parameter-form }

    writeln(t, 1.1:1:1.1);
                    {^ error:type-mismatch }

    writeln(fi);
           {^ error:type-mismatch }

    writeln(1:1.1);
             {^ error:type-mismatch }

    writeln(1:1:1.1);
               {^ error:disallowed-parameter-form }

    writeln(1.1:1:1.1);
                 {^ error:type-mismatch }
end.
