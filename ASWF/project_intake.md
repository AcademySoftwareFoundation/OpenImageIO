# Project Intake checklist

> This is a draft, copied from the intake checklist of OSL. We will replace it
> with the correct intake checklist when it is provided. But in the mean time,
> this is likely to be substantially similar, so provides us a simulation of
> what to expect and a guide to many of the issues we'll need to resolve to
> complete the project intake and graduate to full "adopted" status.


This is a checklist for TSC's to review as part of the intake process. The TSC should review this entire list during the kickoff meeting. For anything outstanding, create an [issue](../issues) to track and link to it in the list

- Existing Project Governance
  - [X] Project License exists ( [LICENSE.md](../LICENSE.md) ) and is OSI-approved
  - [X] Any third-party components/dependencies included are listed along with their licenses ( [THIRD_PARTY.md](../THIRD_PARTY.md) )
  - [ ] Governance defined, outlining community roles and how decisions are made ( [GOVERNANCE.md](../GOVERNANCE.md) )
  - [X] Contribution Policy defined ( [CONTRIBUTING.md](../CONTRIBUTING.md) )
  - [ ] Code of Conduct defined ( existing or pull from [ASWF Sample Project](https://github.com/AcademySoftwareFoundation/aswf-sample-project/blob/master/CODE_OF_CONDUCT.md) )
  - [X] Release methodology defined	( [RELEASING.md](../docs/RELEASING.md) )
- New Project Governance
  - [ ] TSC members identified
  - [X] First TSC meeting held
  - [ ] TSC meeting cadence set and added to project calendar
  - [ ] CLA Approved ( if used ) ( [CCLA](CLA-corporate.md) and [ICLA](CLA-individual.md) )
  - Project charter	( [Technical-Charter.md](Technical-Charter.md) )
    - [ ] Approved by TSC
    - [ ] Filed ( create pull request against [foundation repo](https://github.com/AcademySoftwareFoundation/foundation) )
- Current tools
  - [X] Source Control ( https://github.com/OpenImageIO/oiio )
	- [X] Issue/feature tracker ( https://github.com/OpenImageIO/issues )
  - Collaboration tools
    - [X] Mailing lists (old: http://lists.openimageio.org/listinfo.cgi/oiio-dev-openimageio.org )
      - [ ] Move to groups.io ( create [issue on foundation repo](https://github.com/AcademySoftwareFoundation/foundation/issues/new ) to setup/transfer )
    - [X] Slack or IRC ( #openimageio )
  - [X] CI/build environment
    - Already using GitHub Actions CI
- Project assets
  - [ ] Domain name
	- [ ] Logo(s)
	- [ ] Trademarks/mark ownership rights
- Outreach
  - [ ] New project announcement done 
  - [ ] Project added to ASWF website and ASWF landscape
- Graduation
  - [ ] OpenSSF Best Practices Badge achieved
  - [ ] Committer Diversity	established
	- [ ] Commit/Contribution growth during incubation
	- [ ] Committers defined in the project	( [COMMITTERS.csv](COMMITTERS.csv) or [COMMITTERS.yml](COMMITTERS.yml) )
  - [X] TAC representative appointed
    - Danny Greenstein, TSC chair (possibly interim?), is our TAC rep for the project
  - [ ] License scan completed and no issues found
  - [ ] Code repository imported to ASWF GitHub organization
  - [ ] Developer Certificate of Origin post commit signoff done and DCO Probot enabled.
