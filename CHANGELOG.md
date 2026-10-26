# Changelog

## [1.3.0](https://github.com/embedded-pro/e-foc/compare/v1.2.1...v1.3.0) (2026-10-26)


### Features

* **can:** Choose controllers per mode and set every setting and motor parameter over CAN (contract 2.1) ([#429](https://github.com/embedded-pro/e-foc/issues/429)) ([5f2fca4](https://github.com/embedded-pro/e-foc/commit/5f2fca42859dc650c24cdd35977a781d11c316e1))
* **can:** Contract 2.2 and CAN-only SIL/HIL ([#430](https://github.com/embedded-pro/e-foc/issues/430)) ([0884e8c](https://github.com/embedded-pro/e-foc/commit/0884e8c0b21b1c1924b43ffcda475e322ac20fe9))

## [1.2.1](https://github.com/embedded-pro/e-foc/compare/v1.2.0...v1.2.1) (2026-10-24)


### Bug Fixes

* **can_commander:** Build the Windows tools with MSVC again ([adcf477](https://github.com/embedded-pro/e-foc/commit/adcf477e101908ba2e7a9f41b5d2039a9d951467))


### Continuous Integration

* Build the Linux and Windows tools the same way in CI and in the release ([adcf477](https://github.com/embedded-pro/e-foc/commit/adcf477e101908ba2e7a9f41b5d2039a9d951467))

## [1.2.0](https://github.com/embedded-pro/e-foc/compare/v1.1.0...v1.2.0) (2026-10-24)


### Features

* **can_commander:** Install updates from inside the tool and restart ([#423](https://github.com/embedded-pro/e-foc/issues/423)) ([efe6db6](https://github.com/embedded-pro/e-foc/commit/efe6db6a45a3b4ad2111020d028924cbae945760))
* **can_commander:** Use can-lite's host drivers for slcan, candleLight, PCAN and Kvaser ([#425](https://github.com/embedded-pro/e-foc/issues/425)) ([c8b35c4](https://github.com/embedded-pro/e-foc/commit/c8b35c476fe7864fa7056d5e04678967791a889c))


### Continuous Integration

* **release:** Attest release provenance and sign Windows installers through SignPath ([#422](https://github.com/embedded-pro/e-foc/issues/422)) ([9cef04f](https://github.com/embedded-pro/e-foc/commit/9cef04fc58e75191be66fb3f9af8f8bbf1bce3b3))

## [1.1.0](https://github.com/embedded-pro/e-foc/compare/v1.0.0...v1.1.0) (2026-10-24)


### Features

* **can_commander:** Installers, multi-motor tabs, issue log and update check ([#418](https://github.com/embedded-pro/e-foc/issues/418)) ([5e12c28](https://github.com/embedded-pro/e-foc/commit/5e12c2895d670ff0a4dca889ba176f55b2923e52))
* **can:** Derive the CAN node ID from the board ID jumpers ([#421](https://github.com/embedded-pro/e-foc/issues/421)) ([648bf00](https://github.com/embedded-pro/e-foc/commit/648bf00a6d566b559e9f68eaf913ddbf0d65a6f7))


### Bug Fixes

* **can_commander:** Run the CAN protocol client's timers on the GUI thread; main created no timer service and the client aborted in its constructor ([5e12c28](https://github.com/embedded-pro/e-foc/commit/5e12c2895d670ff0a4dca889ba176f55b2923e52))


### Continuous Integration

* **release:** Bundle documentation PDFs into one zip ([#420](https://github.com/embedded-pro/e-foc/issues/420)) ([4cc1b7b](https://github.com/embedded-pro/e-foc/commit/4cc1b7ba2e8ccc0436f6f6c67af220db6c774c2f))

## [1.0.0](https://github.com/embedded-pro/e-foc/compare/v0.0.1...v1.0.0) (2026-10-23)


### ⚠ BREAKING CHANGES

* FOC motor CAN contract major version is now 2; the MechanicalParamsResponse (0x87) layout changed.
* **platform:** PlatformFactory no longer provides Watchdog() or ResetFromWatchdogExpiry(), and control-loop progress no longer gates the watchdog. The hardware_test "watchdog <ms>" command is removed.
* **state_machine:** validate the lifecycle table at compile time against declared rules ([#325](https://github.com/embedded-pro/e-foc/issues/325))
* raw kp/ki/kd are no longer accepted anywhere in the FOC API; loops are tuned by closed-loop bandwidth in rad/s.

### Features

* Add can bus protocol ([#92](https://github.com/embedded-pro/e-foc/issues/92)) ([1530324](https://github.com/embedded-pro/e-foc/commit/1530324802946157a0d65dead76a81cea01eac57))
* Add dc motor ([81fd4ab](https://github.com/embedded-pro/e-foc/commit/81fd4ab4dcd98665a48f3fddcbf618ec2a24fba8))
* Add documentation for advanced control techniques ([#226](https://github.com/embedded-pro/e-foc/issues/226)) ([b8903cf](https://github.com/embedded-pro/e-foc/commit/b8903cf7a69a3f08670a0dfb8f097edb5171b45a))
* Add error handling implementation ([#137](https://github.com/embedded-pro/e-foc/issues/137)) ([581b351](https://github.com/embedded-pro/e-foc/commit/581b35192e83f3241f858b504e6ed5b4c7168233))
* Add foc state machine ([#127](https://github.com/embedded-pro/e-foc/issues/127)) ([b19b17c](https://github.com/embedded-pro/e-foc/commit/b19b17c641a357e7c1cb30c8b2c40ab96d129f8d))
* Add integration tests ([#129](https://github.com/embedded-pro/e-foc/issues/129)) ([7c8ed2c](https://github.com/embedded-pro/e-foc/commit/7c8ed2ccbf6ead4638bbec6ecdf8ff77696cb283))
* Add mechanical estimation ([#100](https://github.com/embedded-pro/e-foc/issues/100)) ([29f510d](https://github.com/embedded-pro/e-foc/commit/29f510d473adb85fed80a459548e71cfedb16bc8))
* Add new targets ([f6685f8](https://github.com/embedded-pro/e-foc/commit/f6685f8b44c5ae90ced1f0e1751e287be1ab35ab))
* Add nvm ([#119](https://github.com/embedded-pro/e-foc/issues/119)) ([e41fdf3](https://github.com/embedded-pro/e-foc/commit/e41fdf32719a91b688c97b93113271817be0691c))
* Add position controller ([#91](https://github.com/embedded-pro/e-foc/issues/91)) ([4db7d57](https://github.com/embedded-pro/e-foc/commit/4db7d57af346749f570cc42c94c38f7f66384431))
* Bldc implementation ([0ecbb85](https://github.com/embedded-pro/e-foc/commit/0ecbb855b80e2f252ed838b9a3cb127ac4c0cdfb))
* Board identity, named status LEDs, and power status ([#238](https://github.com/embedded-pro/e-foc/issues/238)) ([9c434c3](https://github.com/embedded-pro/e-foc/commit/9c434c31a84b2ef379532398f741589b3db3e21c))
* **calibration:** Measure the current sensors' offsets and remove them from every sample ([#390](https://github.com/embedded-pro/e-foc/issues/390)) ([0b16c1b](https://github.com/embedded-pro/e-foc/commit/0b16c1be0b3304a9df229a2c0a7711c4c309c78a))
* Foc layering ([#229](https://github.com/embedded-pro/e-foc/issues/229)) ([d35be43](https://github.com/embedded-pro/e-foc/commit/d35be4310b42219ace00c47dd4869e5dbee7c002))
* **foc:** Correct every duty for the bridge's dead-time loss ([#391](https://github.com/embedded-pro/e-foc/issues/391)) ([f6dde41](https://github.com/embedded-pro/e-foc/commit/f6dde413cd5c78ffb560c4991e81d062ad1570c3))
* **foc:** Measure the cogging torque at calibration and feed it forward ([#398](https://github.com/embedded-pro/e-foc/issues/398)) ([b6c46f4](https://github.com/embedded-pro/e-foc/commit/b6c46f44cb638d073ee90223a5398d0d861f71f2))
* **foc:** Read two low-side shunts and give the third phase's current from them ([#401](https://github.com/embedded-pro/e-foc/issues/401)) ([84dff48](https://github.com/embedded-pro/e-foc/commit/84dff48832b0d3dc2ff02b6eba421cf22003071f))
* Full refactor of foc ([#33](https://github.com/embedded-pro/e-foc/issues/33)) ([8292cd6](https://github.com/embedded-pro/e-foc/commit/8292cd632e5665a1773aa4c2739a005714785ca3))
* Hil layer  ([#155](https://github.com/embedded-pro/e-foc/issues/155)) ([7d3b655](https://github.com/embedded-pro/e-foc/commit/7d3b6555c3b68f15472b6a34731d326f6f015c9a))
* Implement CAN FOC service layer  ([#239](https://github.com/embedded-pro/e-foc/issues/239)) ([c29f690](https://github.com/embedded-pro/e-foc/commit/c29f6906775abb48f69a4e52288d816560d7bba8))
* Implement deferred stub commands in FocMotorCanBridge ([#241](https://github.com/embedded-pro/e-foc/issues/241)) ([9df8ff8](https://github.com/embedded-pro/e-foc/commit/9df8ff8a0f0f1e861ae708308fe012bc3c84ef1c))
* Improve hardware integration for tiva ([#168](https://github.com/embedded-pro/e-foc/issues/168)) ([7af5b76](https://github.com/embedded-pro/e-foc/commit/7af5b76038795b3bd971f282be98a26cd0def507))
* Motor parameter identification ([#71](https://github.com/embedded-pro/e-foc/issues/71)) ([e832736](https://github.com/embedded-pro/e-foc/commit/e83273669a868caff81365f81950618206d27e47))
* **platform:** Add a watchdog port and bump hal-ti to its WatchDog driver ([#303](https://github.com/embedded-pro/e-foc/issues/303)) ([56abcf4](https://github.com/embedded-pro/e-foc/commit/56abcf4b4c6a9238f2229d7a2ddfd8b88e310448))
* **platform:** Supervise the event dispatcher with the MCU watchdog ([#326](https://github.com/embedded-pro/e-foc/issues/326)) ([7dc0728](https://github.com/embedded-pro/e-foc/commit/7dc07286d19c110d50d557b26310369ec5edaf5b))
* Refactor classes in order to improve testability/readability ([91dac15](https://github.com/embedded-pro/e-foc/commit/91dac1562283a16887ca8627524db00ea5337495))
* Replace monolithic R/L estimator with dual-method services and Goertzel inductance injection ([#251](https://github.com/embedded-pro/e-foc/issues/251)) ([5cfd307](https://github.com/embedded-pro/e-foc/commit/5cfd3070f349164ae7dca473fec6fbaad03081f8))
* Run SIL from an emulator ([#249](https://github.com/embedded-pro/e-foc/issues/249)) ([3e44762](https://github.com/embedded-pro/e-foc/commit/3e447621621422af18f67ffb6459a88fb02de8da))
* **sil:** Make the motor plant an input and cover every control mode, controller and failure path ([#311](https://github.com/embedded-pro/e-foc/issues/311)) ([5626e7a](https://github.com/embedded-pro/e-foc/commit/5626e7a672190e454de83c0eb486799718f429ef))
* **sil:** Put a power stage with dead time between the duty and the winding ([#388](https://github.com/embedded-pro/e-foc/issues/388)) ([6b55509](https://github.com/embedded-pro/e-foc/commit/6b5550926d8d162f3a7731fda8f57508f7473a68))
* **sil:** Read the phase currents through the bench's shunts, amplifiers and converter ([#389](https://github.com/embedded-pro/e-foc/issues/389)) ([fa0d9e1](https://github.com/embedded-pro/e-foc/commit/fa0d9e160f24ad84f3cc4ec413329aec106ae71a))
* **sil:** Step response, disturbance rejection and parameter identification on two reference motors ([#318](https://github.com/embedded-pro/e-foc/issues/318)) ([84f178f](https://github.com/embedded-pro/e-foc/commit/84f178f1853c5ce69e107ad37118240830673e5b))
* **sil:** Time the control interrupt and fix the two overruns it finds ([#334](https://github.com/embedded-pro/e-foc/issues/334)) ([a25623a](https://github.com/embedded-pro/e-foc/commit/a25623a9757857f968193f685c46b96d044c2f3f))
* **state_machine:** Validate the lifecycle table at compile time against declared rules ([#325](https://github.com/embedded-pro/e-foc/issues/325)) ([6d910bc](https://github.com/embedded-pro/e-foc/commit/6d910bcf41dc1a644adb374e19a2be297348faa5))
* **supervision:** Feed the watchdog from proven control-system health ([#268](https://github.com/embedded-pro/e-foc/issues/268)) ([#304](https://github.com/embedded-pro/e-foc/issues/304)) ([1dea864](https://github.com/embedded-pro/e-foc/commit/1dea864e41bc88cb1748389d6323bc8beb7fe179))


### Bug Fixes

* Abort calibration services and release phase-current slot ([#258](https://github.com/embedded-pro/e-foc/issues/258)) ([cbb4528](https://github.com/embedded-pro/e-foc/commit/cbb4528d55e2549622f9e9bc2faacd55f26e0fa0))
* Account every NVM writer, keep busy calibration saves, and guard identification overlap ([#330](https://github.com/embedded-pro/e-foc/issues/330)) ([30330f2](https://github.com/embedded-pro/e-foc/commit/30330f27c21901d103b3d65e476f8d00364b96fd))
* Add wire contract definition and payload validation for FOC motor CAN ([#310](https://github.com/embedded-pro/e-foc/issues/310)) ([df42c8e](https://github.com/embedded-pro/e-foc/commit/df42c8eee7382feb9989155a699d25779c948caa))
* Address critical and high-severity findings from [#255](https://github.com/embedded-pro/e-foc/issues/255) ([#256](https://github.com/embedded-pro/e-foc/issues/256)) ([b31502e](https://github.com/embedded-pro/e-foc/commit/b31502e8476686ec4c501abf5d2ea4aae5ea3add))
* **calibration:** Make full mechanical calibration operational and validated ([#272](https://github.com/embedded-pro/e-foc/issues/272)) ([#301](https://github.com/embedded-pro/e-foc/issues/301)) ([bbdfe95](https://github.com/embedded-pro/e-foc/commit/bbdfe95d585fe88aff5481707a0c436fd19ba3a5))
* **calibration:** Measure the current offsets only once the rotor is at rest ([#413](https://github.com/embedded-pro/e-foc/issues/413)) ([5135f6e](https://github.com/embedded-pro/e-foc/commit/5135f6ef4f1ca46bf5bbe0d000a243a13252c60a))
* **calibration:** Prevent use-after-free when mode selected during pending mech-ident dispatch ([#286](https://github.com/embedded-pro/e-foc/issues/286)) ([9bae8bc](https://github.com/embedded-pro/e-foc/commit/9bae8bce19653f3b3afc607571f6b9e1a38c1449))
* Close ISR setpoint tear, alignment wrap, lost algorithm persists and host no-op ([#329](https://github.com/embedded-pro/e-foc/issues/329)) ([487af99](https://github.com/embedded-pro/e-foc/commit/487af991a9af41c31899a0c75ea0bbc0d2c581f7))
* Code quality and safety improvements for issues [#287](https://github.com/embedded-pro/e-foc/issues/287)–[#292](https://github.com/embedded-pro/e-foc/issues/292) ([#298](https://github.com/embedded-pro/e-foc/issues/298)) ([4fa5878](https://github.com/embedded-pro/e-foc/commit/4fa58789d56ebb4c08ee73d492aaf2ebde83134c))
* **cogging:** Keep a cogging map only when both directions agree on it ([#403](https://github.com/embedded-pro/e-foc/issues/403)) ([3570267](https://github.com/embedded-pro/e-foc/commit/3570267ed415d9824255bf354a056a0602398a09))
* **core:** CLI parsing, units, speed loop, fault latch/budget, external calibration lifecycle ([#411](https://github.com/embedded-pro/e-foc/issues/411)) ([42754b0](https://github.com/embedded-pro/e-foc/commit/42754b06a1f3196c3939c410dd5f3b7c676a9f6b))
* **deps:** Update all submodules to latest main ([#333](https://github.com/embedded-pro/e-foc/issues/333)) ([cd20e65](https://github.com/embedded-pro/e-foc/commit/cd20e657b5d94d132017c2309b27b44a701cee3b))
* **docs:** Copy TikZ SVGs to site and rewrite paths for HTML output ([#254](https://github.com/embedded-pro/e-foc/issues/254)) ([86b4859](https://github.com/embedded-pro/e-foc/commit/86b48597c07f117fd1d501ee4f42fcb25223eba5))
* **docs:** Render TikZ figures to SVG for HTML booklet output ([#253](https://github.com/embedded-pro/e-foc/issues/253)) ([2834596](https://github.com/embedded-pro/e-foc/commit/28345960f5341e03ed8c888dabe48803bca230d0))
* **elec-ident:** Hold the resistance step at mid-rail and rewrite it every sample, so the dead-time correction reaches it ([#405](https://github.com/embedded-pro/e-foc/issues/405)) ([cdb55ff](https://github.com/embedded-pro/e-foc/commit/cdb55ff3883800a59647c78e91cae6edab6677cf))
* **elec-ident:** Steady the online inductance with a 2 s memory and a 50 Hz d-axis excitation ([#397](https://github.com/embedded-pro/e-foc/issues/397)) ([712f24f](https://github.com/embedded-pro/e-foc/commit/712f24f4c56de1f91da67f8b09a71ab27aa7e3cc))
* Fix hw configuration ([#74](https://github.com/embedded-pro/e-foc/issues/74)) ([e00421e](https://github.com/embedded-pro/e-foc/commit/e00421e5d530cc9b0410fcd8b156bff6b176c819))
* **foc:** Pair the online electrical estimate with the duty that acted ([#395](https://github.com/embedded-pro/e-foc/issues/395)) ([45dcaa4](https://github.com/embedded-pro/e-foc/commit/45dcaa4b36719143709af98902057a16294c9647))
* **foc:** Root-cause and fix the LQI speed loop [@sil-known-defect](https://github.com/sil-known-defect) ([#321](https://github.com/embedded-pro/e-foc/issues/321)) ([e72fd08](https://github.com/embedded-pro/e-foc/commit/e72fd080dc183294f454e01b5af92c9ab9937b8a))
* **foc:** Run the online estimator's d-axis test current in bursts ([#400](https://github.com/embedded-pro/e-foc/issues/400)) ([f5dda84](https://github.com/embedded-pro/e-foc/commit/f5dda84e9ed7d0fe00bcf23d02d782c02fa16a94))
* **foc:** Stop the PID position integral winding while the rotor stands still ([#393](https://github.com/embedded-pro/e-foc/issues/393)) ([b3c1efa](https://github.com/embedded-pro/e-foc/commit/b3c1efacdbcf101d539cb2cd5af957a01afb08c8))
* High-severity findings from [#255](https://github.com/embedded-pro/e-foc/issues/255) — real-time, control, docs and metrics ([#259](https://github.com/embedded-pro/e-foc/issues/259)) ([e8d68b3](https://github.com/embedded-pro/e-foc/commit/e8d68b3160322413df5d0e5c11ed02f07f0b3212))
* Hil bringup ident watchdog ([#351](https://github.com/embedded-pro/e-foc/issues/351)) ([bbf94a3](https://github.com/embedded-pro/e-foc/commit/bbf94a3b3dd9cc9ee8a54b64429acc59080d509f))
* **identification:** Deliver identification outcomes from the event loop ([#336](https://github.com/embedded-pro/e-foc/issues/336)) ([4cc7072](https://github.com/embedded-pro/e-foc/commit/4cc70725d810acc3f69f65d40b5238b8650e4e46))
* **identification:** Make the online mechanical estimator converge and publish on the bench motor ([#359](https://github.com/embedded-pro/e-foc/issues/359)) ([19e11e9](https://github.com/embedded-pro/e-foc/commit/19e11e9fe8e455224f6f6dc28207d962fc210c8b))
* **identification:** Make the pole-pair sweep immune to where the rotor rests ([#355](https://github.com/embedded-pro/e-foc/issues/355)) ([4245399](https://github.com/embedded-pro/e-foc/commit/42453992d34e30174fc6ec8f948beb9e790b90ab))
* **identification:** Size the window from the noise the encoder shows ([#354](https://github.com/embedded-pro/e-foc/issues/354)) ([f5d4bb4](https://github.com/embedded-pro/e-foc/commit/f5d4bb4d4a3ed6842eaab5f05bfeff0e8e76c194))
* **identification:** Take the online inductance against the excitation, not the noisy samples ([#386](https://github.com/embedded-pro/e-foc/issues/386)) ([e061c5b](https://github.com/embedded-pro/e-foc/commit/e061c5ba3017f33127c8582a7b027a8c7de30afc))
* **identification:** Weight the torque by the triangle the acceleration carries ([#352](https://github.com/embedded-pro/e-foc/issues/352)) ([d41b267](https://github.com/embedded-pro/e-foc/commit/d41b2679a54327844fd8db4480e8207c049b073c))
* Implement verified remediation findings from audit ([#236](https://github.com/embedded-pro/e-foc/issues/236)) ([4416c85](https://github.com/embedded-pro/e-foc/commit/4416c85eaeaaa2333c3219a019bcb2b4552042eb))
* **integration_tests:** Read the target's answers on hardware ([#358](https://github.com/embedded-pro/e-foc/issues/358)) ([bcf032e](https://github.com/embedded-pro/e-foc/commit/bcf032ee3a4d379adea942d1093b98f757658620))
* Keep faults across mode switches and carry identification results over CAN ([#328](https://github.com/embedded-pro/e-foc/issues/328)) ([53bf2c2](https://github.com/embedded-pro/e-foc/commit/53bf2c27d6633e9a5746c60abab3e165948a4dea))
* **mech-ident:** Fit each direction of rotation separately in the online friction estimate ([#394](https://github.com/embedded-pro/e-foc/issues/394)) ([8f50956](https://github.com/embedded-pro/e-foc/commit/8f50956cf6baa259966efc69fcb89413d66ae299))
* **mech-ident:** Fit the friction over whole turns of the rotor ([#407](https://github.com/embedded-pro/e-foc/issues/407)) ([af26330](https://github.com/embedded-pro/e-foc/commit/af263303e8f83b360259d0683370917307f3d3fd))
* **mech-ident:** Fit the identification windows from the event loop ([#414](https://github.com/embedded-pro/e-foc/issues/414)) ([f70e410](https://github.com/embedded-pro/e-foc/commit/f70e410ee2e3019d5fc05cdfdb18f903e4466d43))
* **mech-ident:** Publish the online friction from constant-speed runs, pooled over both directions ([#417](https://github.com/embedded-pro/e-foc/issues/417)) ([ff1a228](https://github.com/embedded-pro/e-foc/commit/ff1a22827fe4326bec51d766b8fe672af632a921))
* **platform:** Reclaim SRAM held by duplicated and unreachable storage ([#307](https://github.com/embedded-pro/e-foc/issues/307)) ([1c942ec](https://github.com/embedded-pro/e-foc/commit/1c942ec76804c63c34bcc69530a27d5686b5452f))
* **platforms:** Start the ST application, cap FRDM current, sample the 123 low side at zero, guard the TI encoder ([#412](https://github.com/embedded-pro/e-foc/issues/412)) ([54a7cab](https://github.com/embedded-pro/e-foc/commit/54a7cabc1fe54f04d8506478dc87975816a3d69a)), closes [#360](https://github.com/embedded-pro/e-foc/issues/360) [#363](https://github.com/embedded-pro/e-foc/issues/363) [#364](https://github.com/embedded-pro/e-foc/issues/364) [#368](https://github.com/embedded-pro/e-foc/issues/368)
* **position:** Report no demanded speed while LQR or LQI commands the current ([#404](https://github.com/embedded-pro/e-foc/issues/404)) ([b424240](https://github.com/embedded-pro/e-foc/commit/b4242401e466efc6caa32b18aae05ce1df40fa2c))
* PWM bootstrap and no-sample watchdog for R/L identification ([#285](https://github.com/embedded-pro/e-foc/issues/285)) ([e562ec1](https://github.com/embedded-pro/e-foc/commit/e562ec11ea9626deb8def9fc7ec298af7b809f1d))
* Reject CAN electrical identification outside stopped lifecycle states ([#282](https://github.com/embedded-pro/e-foc/issues/282)) ([51da155](https://github.com/embedded-pro/e-foc/commit/51da155807c33fabf7c4b1bc13bf695fc93edee2))
* Require valid rotor reference before enabling after reset ([#284](https://github.com/embedded-pro/e-foc/issues/284)) ([19446ee](https://github.com/embedded-pro/e-foc/commit/19446eea6834b324fce6fc6ffe252516eba6aa51))
* Resolve contradictory requirements, design docs and CAN wire format ([#280](https://github.com/embedded-pro/e-foc/issues/280)) ([#281](https://github.com/embedded-pro/e-foc/issues/281)) ([6b6768e](https://github.com/embedded-pro/e-foc/commit/6b6768ef93bf7def5c1b98cf112095d240a918ea))
* Root-cause every held-out SIL scenario and run the whole suite by default ([#324](https://github.com/embedded-pro/e-foc/issues/324)) ([9fface9](https://github.com/embedded-pro/e-foc/commit/9fface963ab37e43a600013081101528010eb527))
* **safety:** Prevent partial calibration from entering Ready ([#283](https://github.com/embedded-pro/e-foc/issues/283)) ([ce1afab](https://github.com/embedded-pro/e-foc/commit/ce1afabe4f47721285a82befd8d31fab342f4b03))
* **sil:** Let the plant drive its windings through the power stage and read them through the sense chain together ([#408](https://github.com/embedded-pro/e-foc/issues/408)) ([7174d2a](https://github.com/embedded-pro/e-foc/commit/7174d2ae101dd5ca5d8e300e8b8522e3e4c9f26e))
* **state_machine:** Make fault observation atomic across the enable sequence ([#271](https://github.com/embedded-pro/e-foc/issues/271)) ([#305](https://github.com/embedded-pro/e-foc/issues/305)) ([d5343d3](https://github.com/embedded-pro/e-foc/commit/d5343d3a72892336b6e058832492592f3686cd38))
* **ti:** Keep every phase readable by its low-side shunt ([#357](https://github.com/embedded-pro/e-foc/issues/357)) ([302a2fe](https://github.com/embedded-pro/e-foc/commit/302a2feb631cd04b3beb7ac08385af19d69eb58a))
* **ti:** Read the last phase current inside its low-side window ([#387](https://github.com/embedded-pro/e-foc/issues/387)) ([58139fc](https://github.com/embedded-pro/e-foc/commit/58139fc045bd83dc0991336bd5d348e7e5b93e5d))
* **ti:** Trip on any phase current beyond the board's over-current threshold ([#392](https://github.com/embedded-pro/e-foc/issues/392)) ([aa934e8](https://github.com/embedded-pro/e-foc/commit/aa934e88296003caa1c230734741ddb5a55a678b))
* **ti:** Trip the bridge on bus undervoltage and trace the fault code ([#356](https://github.com/embedded-pro/e-foc/issues/356)) ([71ecfc2](https://github.com/embedded-pro/e-foc/commit/71ecfc2bbab1011ea161ab29b0ab1ee99c230af6))
* **tools:** Can_commander node routing/busy/rounding/decoding, hardware_bridge connect and send lifetime; float test asserts ([#410](https://github.com/embedded-pro/e-foc/issues/410)) ([db5e389](https://github.com/embedded-pro/e-foc/commit/db5e38970533bc6a1751798b5fcf002c9a9fba48))
* **tuning:** Apply shared finite bounds and stopped-state policy to tuning ingress ([#273](https://github.com/embedded-pro/e-foc/issues/273)) ([#299](https://github.com/embedded-pro/e-foc/issues/299)) ([2c0756f](https://github.com/embedded-pro/e-foc/commit/2c0756f071b612003da39b9d5d64bcdd05a3243d))


### Performance Improvements

* Cut RAM to ~25 KB and flash to ~204 KB on the EK-TM4C123GXL ([#416](https://github.com/embedded-pro/e-foc/issues/416)) ([e6cbcf7](https://github.com/embedded-pro/e-foc/commit/e6cbcf7b22c69ee3f50159428ce64503cc5f67a9))
* **ti:** Take hal-ti's comparator-only PWM writes ([#415](https://github.com/embedded-pro/e-foc/issues/415)) ([514dc56](https://github.com/embedded-pro/e-foc/commit/514dc56e99fd2a07fc20c5636b55ba557e3324b0))
