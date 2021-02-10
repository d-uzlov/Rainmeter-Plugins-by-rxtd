# Expression parsing

## Historical background

Originally project PerfMonRxtd used expression trees to allow users
to calculate arrays of values using dynamically-resolved values.
Syntax for those "dynamically-resolved values" was designed very poorly
which resulted in some very strange constructions in expression parser,
which prevented the project from using expression parsing libraries.

Also is was a great opportunity for expression parsing excercise.
Although result wasn't state of the art, it was good enough for the project's purposes.

&nbsp;
&nbsp;

Then in the process of creating convenient way to parse complex options for AudioAnalyzer
expression parsing was repurposed as a pure math calculator,
without any dynamic resolution of values.

&nbsp;
&nbsp;

Since expression parsing was written many years ago
and hasn't really changed ever since its creation,
code quality there didn't quite meet the raised quality standards of the project.
And since it was not very high tech from the start,
simple code cleanup wouldn't make it much better,
so finding some alternative implementation was the better option.

Maybe using some library for the task would have been the better solution
in terms of performance and/or code support, 
but writing it by hand was another great opportunity for expression parsing excercise.
Now with higher coding standards and better algorithms.
