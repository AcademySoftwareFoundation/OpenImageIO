<!-- SPDX-License-Identifier: CC-BY-4.0 -->
<!-- Copyright Contributors to the OpenImageIO Project. -->

# OpenImageIO Project Governance

OpenImageIO is a project of the Academy Software Foundation and relies on the
ASWF governance policies, supported by the Linux Foundation.

There are three primary project roles: Contributors submit code to the
project; Committers approve code to be included into the project; and the
Technical Steering Committee (TSC) provides overall high-level project
guidance.

* [Contributors](#Contributors)
* [Committers](#Committers)
* [Technical Steering Committee](#Technical-Steering-Committee)

## Contributors

The OpenImageIO project grows and thrives from assistance from Contributors.
Contributors include anyone in the community who contributes code,
documentation, or other technical artifacts that have been incorporated into
the project repository.

Anyone can be a Contributor. You need no formal approval from the project,
beyond the legal forms.

### How to Become a Contributor

* Review the coding standards and [contributing
  guidelines](../CONTRIBUTING.md) to ensure your contribution is in line with
  the project's coding and styling guidelines.

* Sign the Individual CLA, or if you are employed by an organization that
  might have any claim to IP you create, have your organization sign the
  Corporate CLA.

* Submit your code as a PR with the appropriate DCO sign-off on each commit.
  The easiest way to do this is to ensure that you commit your code with `git
  commit -s`.

## Committers

Project Committers have merge access on the OpenImageIO GitHub repository
and are responsible for approving submissions by Contributors.

### Committer Responsibilities

Typical activities of a Committer include:

* Helping users and novice contributors.

* Ensuring a response to questions posted to the
  [Oiio-dev developer mail list](https://lists.aswf.io/g/oiio-dev)

* Contributing code and documentation changes that improve the project.

* Reviewing and commenting on issues and pull requests.

* Ensuring that changes and new code meet acceptable standards and are in
  the long-term interest of the project.

* Participation in working groups.

* Merging pull requests.

### How to Become a Committer

Any member of the OpenImageIO community (though typically an existing
Committer or TSC member) may nominate an individual making significant and
valuable contributions to the OpenImageIO project to become a new Committer.
To nominate a new Committer, open an issue in the OpenImageIO repository, send
mail to the TSC mail list, or raise the issue at a TSC meeting.

The TSC may periodically review the Committer list to identify inactive
Committers. Past Committers are typically given Emeritus status. Emeriti may
request that the TSC restore them to active Committer status.

## Technical Steering Committee

The Technical Steering Committee (TSC) has final authority over this project.
As defined in the project charter, in addition to committer activities, TSC
responsibilities also include, but are not limited to:

* Coordinating technical direction of the Project.

* Project governance and contribution policy.

* GitHub repository administration.

* Maintaining the list of additional Committers

* Appointing representatives to work with other open source or open
  standards communities.

* Discussions, seeking consensus, and where necessary, voting on technical
  matters relating to the code base that affect multiple projects.

* Coordinating any marketing, events, or communications regarding the
  project.

Within the TSC are two elected leadership roles to be held by its members
and voted on annually. Any TSC member can express interest in serving in a
role, or nominate another member to serve. There are no term limits, and one
person may hold multiple roles simultaneously. Should a TSC member resign
from a leadership role before their term is complete, a successor shall be
elected through the standard nomination and voting process to complete the
remainder of the term. The leadership roles are:

* **Chair**: This position acts as the project manager, organizing meetings
  and providing oversight to project administration.

* **Chief Architect**: This position makes all the final calls on design and
  technical decisions, and is responsible for avoiding "design by committee"
  pitfalls.

The chair role is assumed to rotate annually (though there are no term
limits, so the TSC may reelect an existing chair). The chief architect
position should be a source of stability and coherent design vision, so the
TSC is encouraged to choose an architect who can serve for many years and
only change architects when it is necessary for the health of the project
and its community.

At the time of election, the TSC will also agree upon which of these two
leaders will serve as the OpenImageIO ASWF (Academy Software Foundation) TAC
(Technical Advisory Council) representative for the term. This member
represents the project at all ASWF TAC meetings.

### TSC Members

Current voting members of the TSC are:

* **Chair and TAC representative**: Daniel Greenstein - Sony Pictures Imageworks (possibly interim?)
* **Chief Architect**: Larry Gritz - Sony Pictures Imageworks
* Anton Dukhovnikov - Wētā Digital / Unity
* Jesse Yurkovich - Blender
* Joseph Goldstone - ARRI
* Lewis Wakeland - Disney TV Animation
* Luke Emrose - Animal Logic (interim?)
* Martin Davies - LAIKA
* Robin Rowe - Heroic Robots
* Thiago Ize - Autodesk


### TSC Nomination and Succession

Any proposal for additional members of the TSC may be submitted by Committers,
TSC members, or other major stakeholders of the OpenImageIO community by
opening an issue outlining their case or raising the issue at a TSC meeting.
New TSC members are accepted or rejected by majority vote of the TSC.

If a TSC member is for an extended period not regularly participating or
performing the responsibilities expected of TSC members, the TSC may by
majority vote request an alternate TSC member be submitted by that
organization, or remove the inactive member from the TSC.

A voting member of the TSC may nominate a successor in the event that such
voting member decides to leave the TSC, and the TSC, including the departing
member, shall confirm or reject such nomination by a vote. In the event that
the departing member's nomination for successor is rejected by vote of the
TSC, the departing member shall be entitled to continue nominating successors
until one such successor is confirmed by vote of the TSC. If the departing
member fails or is unable to nominate a successor, the TSC may nominate one on
the departing member's behalf.

TSC membership is presumed to be retained by the individual even if they
change employers, provided they remain active in the project. The TSC may take
action to ensure that organizational stakeholder representation not become
severely disproportionate, for example by urging an organization that loses
its sole TSC representative to nominate a new member, or by limiting the total
number of voting members from any one organization if too many members all
move to the same organization.

### TSC Meetings

Any meetings of the TSC are intended to be open to the public, except where
there is a reasonable need for privacy. The TSC meets regularly in a voice
conference call, at a cadence deemed appropriate by the TSC. The TSC Chair
moderates the meeting, or appoints another TSC member to moderate in his or
her absence. Meetings may also be streamed online where appropriate;
connection details will be posted to the oiio-dev mail list in advance of the
scheduled meeting.

The biweekly TSC meetings can be found on the [ASWF Calendar](https://calendar.openimageio.org),
just click on the OpenImageIO TSC meeting entries to get the Zoom link.

Items are added to the TSC agenda which are considered contentious or are
modifications of governance, contribution policy, TSC membership, or release
process, in addition to topics involving the high-level technical direction
of the project.

The intention of the agenda is not to approve or review all patches. That
should happen continuously on GitHub and be handled by the larger group of
Committers.

Any community member or Contributor can ask that something be reviewed by
the TSC at the meeting by logging a GitHub issue. Any Committer, TSC member,
or the meeting chair can bring the issue to the TSC's attention by applying
the `TSC` label.

Prior to each TSC meeting, the meeting chair will share the agenda with
members of the TSC. TSC members can also add items to the agenda at the
beginning of each meeting. The meeting chair and the TSC cannot veto or
remove items.

The TSC may invite additional persons to participate in a non-voting
capacity.

The meeting chair is responsible for ensuring that minutes are taken and
archived in the project repository or other designated accessible location.

Due to the challenges of scheduling a global meeting with participants in
several time zones, the TSC will seek to resolve as many agenda items as
possible outside of meetings on the public mailing list or through GitHub
issues, discussions, and pull requests.
