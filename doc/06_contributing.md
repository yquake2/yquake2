# Contributing Code

You want to contribute code to Yamagi Quake II? That's good! We're
always interested in contributions, as long as they're in the scope of
our little project. It doesn't matter if you're sending new features,
bugfixes or even documentation updates.

As a general note: **Ask before writing code!** Nobody was ever hurt for
asking if his or her idea is in scope for Yamagi Quake II. And an early
"no" is always better then rejections after having put tens of hours
into the idea.

Some rules to follow:

* Use GitHub! Sign up for an account, fork our repository and send a
  pull request after you're satisfied with your work. We won't accept
  patches sent by email or - even worse - as pastebin links.
* Create a history of small, distinct commits. Several small commits are
  always better then one big commit. They make your changes more
  understandable and ease debugging.
* Never ever add new dependencies. We won't accept code that add new
  dependencies. If in doubt, because you really need that nice little
  library, ask.
* Make sure that your code is compiling without warnings with at least
  current versions of GCC and Clang. Also make sure that it's working on
  both unixoid platforms and Windows.
* Don't do unnecessary cleanups. Yes, your linter or sanity checker may
  complain. But that's your problem and not ours. Cleanups often bring
  next to no advantage, Quake II has always been a mess and it'll stay a
  mess until the sun collapses. And cleanups are hard to test, often
  introduce new bugs and make debugging harder.
* Stick to the code style of the file you're editing.
