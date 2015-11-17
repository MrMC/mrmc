### Introduction

**MrMC** uses Github for development only, i.e. for *pull requests* and the review of such code.  
**Do not open** an issue on Github for your questions or bug reports.  
**Do not comment** on a *pull request* unless you are involved in the testing of such or have something meaningful to contribute.  
Not familiar with git? Start by looking at Github's [collaborating pages](https://help.github.com/categories/collaborating/).

#### Questions about MrMC?

To get your questions answered, please ask in the [MrMC community forum's](http://forum.mrmc.tv/) or on **IRC:** [#mrmc-public](http://webchat.freenode.net?nick=mrmc-contrib&channels=%23mrmc-public&prompt=1&uio=OT10cnVlde) on freenode.net

#### Issue or bug reports and discussions

Issue or bug reports are created and reviewed at the [MrMC community forum](http://forum.mrmc.tv).
If you can, we encourage you to investigate the issue yourself and create a [pull request](https://help.github.com/articles/creating-a-pull-request/) for us to review.

For bug reports and related discussions, feature requests and all other support, please go to [MrMC community forum's](http://forum.mrmc.tv/).

#### Pull Requests

Before [creating a pull request](https://help.github.com/articles/creating-a-pull-request/), please read our general code guidelines that can be found at [MrMC community forum](http://forum.mrmc.tv):

###### General guidelines for creating pull requests:
- **Create topic branches**. Don't ask us to pull from your master branch. 
- **One pull request per feature**. If you want to do more than one thing, send multiple *pull requests*. 
- **Send coherent history**. Make sure each individual commit in your *pull
  request* is meaningful.  
  If you had to make multiple intermediate commits while developing, please squash them before sending them to us.  
  In the end before merging you may be asked to squash your commit even some more.

###### Please follow these guidelines; it's the best way to get your work included in the project!

- [Click here](https://github.com/mrmc/mrmc/fork/) to **fork MrMC** project,
   and [configure the remote](https://help.github.com/articles/configuring-a-remote-for-a-fork/):

   ```bash
   # Clone your fork of MrMC's repo into the current directory in terminal
   git clone git@github.com:<your github username>/mrmc.git mrmc
   # Navigate to the newly cloned directory
   cd mrmc
   # Assign the original repo to a remote called "upstream"
   git remote add upstream https://github.com/mrmc/mrmc.git
   ```

- If you cloned a while ago, get the latest changes from upstream:

   ```bash
   # Fetch upstream changes
   git fetch upstream
   # Make sure you are on your 'master' branch
   git checkout master
   # Merge upstream changes
   git merge upstream/master
   ```

- Create a new topic branch to contain your feature, change, or fix:

   ```bash
   git checkout -b <topic-branch-name>
   ```

- Commit your changes in logical chunks, or your *pull request* is unlikely to be merged into the main project.  
   Use git's [interactive rebase](https://help.github.com/articles/interactive-rebase)
   feature to tidy up your commits before making them public.

- Push your topic branch up to your fork:

   ```bash
   git push origin <topic-branch-name>
   ```

-  Open a [pull request](https://help.github.com/articles/using-pull-requests) with a 
   clear title and description.
