program bpb(input, output);
type
    tagType = 0..1;
    recType = record
        case tagType of 0: (); 1: ();
    end;
var
    b: boolean;
    c: char;
    i: integer;
    pi: ^integer;
    fi: file of integer;
    t: text;
    x: real;

    pr: ^recType;

    packedArray: packed array ['a'..'f'] of integer;
    packedArrayWrongType: packed array ['a'..'f'] of real;
    unpackedArray: array [0..10] of integer;
    unpackedArrayWrongType: array [0..10] of real;
begin
    { abs }

    i := abs;
           {^ error:parameter-count-mismatch }
    pi := abs(pi);
             {^ error:non-numeric-type }
    i := abs(i:3);
             {^ error:disallowed-parameter-form }
    i := abs(i, i);
               {^ error:parameter-count-mismatch }

    { arctan }

    x := arctan;
              {^ error:parameter-count-mismatch }
    x := arctan(pi);
               {^ error:non-numeric-type }
    x := arctan(x:3);
                {^ error:disallowed-parameter-form }
    x := arctan(x, x);
                  {^ error:parameter-count-mismatch }

    { chr }

    c := chr;
           {^ error:parameter-count-mismatch }
    c := chr(x);
            {^ error:non-integer-type }
    c := chr(i:3);
             {^ error:disallowed-parameter-form }
    c := chr(i, i);
               {^ error:parameter-count-mismatch }

    { cos }

    x := cos;
           {^ error:parameter-count-mismatch }
    x := cos(pi);
            {^ error:non-numeric-type }
    x := cos(x:3);
             {^ error:disallowed-parameter-form }
    x := cos(x, x);
               {^ error:parameter-count-mismatch }

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

    { eof }

    b := eof(0);
            {^ error:disallowed-parameter-form }
    b := eof(i);
            {^ error:type-mismatch }
    b := eof(fi:3);
              {^ error:disallowed-parameter-form }
    b := eof(fi, 1);
                {^ error:parameter-count-mismatch }

    { eoln }

    b := eoln(0);
             {^ error:disallowed-parameter-form }
    b := eoln(fi);
             {^ error:type-mismatch }
    b := eoln(t:3);
              {^ error:disallowed-parameter-form }
    b := eoln(t, 1);
                {^ error:parameter-count-mismatch }

    { exp }

    x := exp;
           {^ error:parameter-count-mismatch }
    x := exp(pi);
            {^ error:non-numeric-type }
    x := exp(x:3);
             {^ error:disallowed-parameter-form }
    x := exp(x, x);
               {^ error:parameter-count-mismatch }

    { get }

    get;
      {^ error:parameter-count-mismatch }
    get(0);
       {^ error:disallowed-parameter-form }
    get(i);
       {^ error:type-mismatch }
    get(fi:3);
         {^ error:disallowed-parameter-form }
    get(fi, 1);
           {^ error:parameter-count-mismatch }

    { ln }

    x := ln;
          {^ error:parameter-count-mismatch }
    x := ln(pi);
           {^ error:non-numeric-type }
    x := ln(x:3);
            {^ error:disallowed-parameter-form }
    x := ln(x, x);
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

    { pack }

    pack;
       {^ error:parameter-count-mismatch }
    pack(i, 1, packedArray);
        {^ error:non-array-type }
    pack(packedArray, 1, packedArray);
        {^ error:type-mismatch }
    pack(unpackedArray:3, 1, packedArray);
                     {^ error:disallowed-parameter-form }

    pack(unpackedArray);
                     {^ error:parameter-count-mismatch }
    pack(unpackedArray, 'a', packedArray);
                       {^ error:type-mismatch }
    pack(unpackedArray, 1:3, packedArray);
                        {^ error:disallowed-parameter-form }

    pack(unpackedArray, 1);
                        {^ error:parameter-count-mismatch }
    pack(unpackedArray, 1, i);
                          {^ error:non-array-type }
    pack(unpackedArray, 1, unpackedArray);
                          {^ error:type-mismatch }
    pack(unpackedArray, 1, packedArrayWrongType);
                          {^ error:type-mismatch }
    pack(unpackedArray, 1, packedArray:3);
                                     {^ error:disallowed-parameter-form }

    pack(unpackedArray, 1, packedArray, 'abc');
                                       {^ error:parameter-count-mismatch }

    { page }

    page(0);
        {^ error:disallowed-parameter-form }
    page(fi);
        {^ error:type-mismatch }
    page(t:3);
         {^ error:disallowed-parameter-form }
    page(t, 1);
           {^ error:parameter-count-mismatch }

    { put }

    put;
      {^ error:parameter-count-mismatch }
    put(0);
       {^ error:disallowed-parameter-form }
    put(i);
       {^ error:type-mismatch }
    put(fi:3);
         {^ error:disallowed-parameter-form }
    put(fi, 1);
           {^ error:parameter-count-mismatch }

    { read (???) }

    read;
       {^ error:parameter-count-mismatch }

    { read (typed) }

    read(fi);
          {^ error:parameter-count-mismatch }
    read(fi:3, i);
          {^ error:disallowed-parameter-form }
    read(fi, 0);
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
    read(t, 0);
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
    readln(t, 0);
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
    reset(0);
         {^ error:disallowed-parameter-form }
    reset(i);
         {^ error:type-mismatch }
    reset(fi:3);
           {^ error:disallowed-parameter-form }
    reset(fi, 1);
             {^ error:parameter-count-mismatch }

    { rewrite }

    rewrite;
          {^ error:parameter-count-mismatch }
    rewrite(0);
           {^ error:disallowed-parameter-form }
    rewrite(i);
           {^ error:type-mismatch }
    rewrite(fi:3);
             {^ error:disallowed-parameter-form }
    rewrite(fi, 1);
               {^ error:parameter-count-mismatch }

    { sin }

    x := sin;
           {^ error:parameter-count-mismatch }
    x := sin(pi);
            {^ error:non-numeric-type }
    x := sin(x:3);
             {^ error:disallowed-parameter-form }
    x := sin(x, x);
               {^ error:parameter-count-mismatch }

    { sqr }

    i := sqr;
           {^ error:parameter-count-mismatch }
    pi := sqr(pi);
             {^ error:non-numeric-type }
    i := sqr(i:3);
             {^ error:disallowed-parameter-form }
    i := sqr(i, i);
               {^ error:parameter-count-mismatch }

    { sqrt }

    x := sqrt;
            {^ error:parameter-count-mismatch }
    x := sqrt(pi);
             {^ error:non-numeric-type }
    x := sqrt(x:3);
              {^ error:disallowed-parameter-form }
    x := sqrt(x, x);
                {^ error:parameter-count-mismatch }

    { unpack }

    unpack;
         {^ error:parameter-count-mismatch }
    unpack(i, unpackedArray, 1);
          {^ error:non-array-type }
    unpack(unpackedArray, unpackedArray, 1);
          {^ error:type-mismatch }
    unpack(packedArray:3, unpackedArray, 1);
                     {^ error:disallowed-parameter-form }

    unpack(packedArray);
                     {^ error:parameter-count-mismatch }
    unpack(packedArray, i, 1);
                       {^ error:non-array-type }
    unpack(packedArray, packedArray, 1);
                       {^ error:type-mismatch }
    unpack(packedArray, unpackedArrayWrongType, 1);
                       {^ error:type-mismatch }
    unpack(packedArray, unpackedArray:3, 1);
                                    {^ error:disallowed-parameter-form }

    unpack(packedArray, unpackedArray);
                                    {^ error:parameter-count-mismatch }
    unpack(packedArray, unpackedArray, 'a');
                                      {^ error:type-mismatch }
    unpack(packedArray, unpackedArray, 1:3);
                                       {^ error:disallowed-parameter-form }

    unpack(packedArray, unpackedArray, 1, 'abc');
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
              {^ error:non-integer-type }
    write(t, 1:1:1.1);
                {^ error:disallowed-parameter-form }
    write(t, 1.1:1:1.1);
                  {^ error:non-integer-type }
    write(pi);
         {^ error:type-mismatch }
    write(1:1.1);
           {^ error:non-integer-type }
    write(1:1:1.1);
             {^ error:disallowed-parameter-form }
    write(1.1:1:1.1);
               {^ error:non-integer-type }

    { writeln }

    writeln(t.x);
            {^ error:non-record-type } { testing that the error is only emitted once }
    writeln(t:3);
            {^ error:disallowed-parameter-form }
    writeln(t, t);
              {^ error:type-mismatch }
    writeln(t, 1:1.1);
                {^ error:non-integer-type }
    writeln(t, 1:1:1.1);
                  {^ error:disallowed-parameter-form }
    writeln(t, 1.1:1:1.1);
                    {^ error:non-integer-type }
    writeln(fi);
           {^ error:type-mismatch }
    writeln(1:1.1);
             {^ error:non-integer-type }
    writeln(1:1:1.1);
               {^ error:disallowed-parameter-form }
    writeln(1.1:1:1.1);
                 {^ error:non-integer-type }
end.
