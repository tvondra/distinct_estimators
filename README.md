Distinct Estimators
===================

This repository contains four PostgreSQL extension, each with
a different statistical estimator, useful for estimating number
of distinct items in a data set.

If you need to track distinct elements, and you can settle with
a reasonably precise estimate, this may be an interesting option.
These extensions may easily give you estimates with about 1% error,
very little memory (usually just few kilobytes) and much faster
than the usual DISTINCT aggregate.

I wrote a [short article](http://www.fuzzy.cz/en/articles/aggregate-functions-for-distinct-estimation/)
about it a while ago and now I've finally finished these extensions.


Usage as an aggregate
---------------------
There are two ways to use those extensions - either as an aggregate
or as a data type (for a column). Let's see the aggregate first ...

There are four extensions, each one provides an aggregate

1. adaptive_distinct(int, real, int)
2. bitmap_distinct(int, real, int)
3. pcsa_distinct(int, int, int)
4. probabilistic_distinct(int, int, int)

and about the same aggregates for text values. The second and third
parameter are parameters of the estimator, usually affecting precision
and memory requirements.

For adaptive and bitmap estimators those parameters are demanded error
rate and expected number of distinct values, so for example to get an
estimate with 1% error and you expect there are 1.000.000 distinct
items, you can do this

    db=# SELECT adaptive_distinct(i, 0.01, 1000000)
         FROM generate_series(1,1000000);

and likewise for the bitmap estimator.

In case of the pcsa/probabilistic estimators, the parameters are not
that straightforward - in most cases you have to play with the
estimator a bit to find out what parameters to use (see more detail
in the readme for the estimators).

Or you may forget about the parameters and use just a simplified
version of the aggregates without the parameters, i.e.

    db=# SELECT adaptive_distinct(i) FROM generate_series(1,1000000);

which uses some carefully selected parameter values, that work quite
well in most cases. But if you want to get the best performance (low
memory requirements, fast execution and precise results), you'll have
to play a bit with the estimators.


Usage as a data type (for a column)
-----------------------------------
Each of the estimators provides a separate data type (based on bytea),
and you may use it as a column in a table or in a PL/pgSQL procedure.
For example to create an table with an adaptive_estimator, you can
do this:

    CREATE TABLE article (
      ...
      cnt_visitors  adaptive_estimator DEFAULT adaptive_init(0.01, 1e6)
      ...
    );

What is it good for? Well, you can continuously update the column and
know how many distinct visitors (by IP or whatever you choose) already
read the article. You can't do that with a plain INT column.

The counter may be easily updated by a trigger, just use the proper 
add_item function to add the items and get_estimate to get the current
estimate. The are a few other functions, e.g. "reset" (that effectively
sets the counter to 0) and "size" (returns memory requirements of an
estimator with supplied parameters).

Anyway be careful about the implementation, as the estimators may
easily occupy several kilobytes (depends on the precision etc.). Keep
in mind that the PostgreSQL MVCC works so that it creates a copy of
the row on update, an that may easily lead to bloat. So group the
updates or something like that.


Differences
-----------
It's difficult to give a clear rule which of the extensions to choose,
I recommend to play with them a bit, try them on an existing data set
and you'll see which one performs best.

Anyway my experiences are that:

1. The higher the precision and expected number of distinct values,
   the more memory / CPU time you'll need.

2. Adaptive/bitmap estimators are generally easier to work with, as
   you can easily set the error rate / expected number of distinct
   values.

3. The pcsa/probabilistic estimators are much simpler and you need
   to play with them to get an idea what the right parameter values
   are.

4. Pcsa/probabilistic estimators usually require less memory than
     the adaptive/bitmap.

5. Adaptive estimator has a very interesting feature - you can
   merge multiple estimators into a single one, providing a global
   distinct estimate. For example you may maintain counters for
   each article category, and later merge the estimators to get
   an estimate for a group of categories.

6. The pcsa tend to give bad results for low numbers of distinct
   values - that's where adaptive/bitmap clearly win.

See READMEs for individual estimators for more details.