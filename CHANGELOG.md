# Changelog

## [1.0.0](https://github.com/embedded-pro/e-foc/compare/v0.0.1...v1.0.0) (2026-08-31)


### ⚠ BREAKING CHANGES

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
* Foc layering ([#229](https://github.com/embedded-pro/e-foc/issues/229)) ([d35be43](https://github.com/embedded-pro/e-foc/commit/d35be4310b42219ace00c47dd4869e5dbee7c002))
* Full refactor of foc ([#33](https://github.com/embedded-pro/e-foc/issues/33)) ([8292cd6](https://github.com/embedded-pro/e-foc/commit/8292cd632e5665a1773aa4c2739a005714785ca3))
* Hil layer  ([#155](https://github.com/embedded-pro/e-foc/issues/155)) ([7d3b655](https://github.com/embedded-pro/e-foc/commit/7d3b6555c3b68f15472b6a34731d326f6f015c9a))
* Implement CAN FOC service layer  ([#239](https://github.com/embedded-pro/e-foc/issues/239)) ([c29f690](https://github.com/embedded-pro/e-foc/commit/c29f6906775abb48f69a4e52288d816560d7bba8))
* Implement deferred stub commands in FocMotorCanBridge ([#241](https://github.com/embedded-pro/e-foc/issues/241)) ([9df8ff8](https://github.com/embedded-pro/e-foc/commit/9df8ff8a0f0f1e861ae708308fe012bc3c84ef1c))
* Improve hardware integration for tiva ([#168](https://github.com/embedded-pro/e-foc/issues/168)) ([7af5b76](https://github.com/embedded-pro/e-foc/commit/7af5b76038795b3bd971f282be98a26cd0def507))
* Motor parameter identification ([#71](https://github.com/embedded-pro/e-foc/issues/71)) ([e832736](https://github.com/embedded-pro/e-foc/commit/e83273669a868caff81365f81950618206d27e47))
* Refactor classes in order to improve testability/readability ([91dac15](https://github.com/embedded-pro/e-foc/commit/91dac1562283a16887ca8627524db00ea5337495))
* Replace monolithic R/L estimator with dual-method services and Goertzel inductance injection ([#251](https://github.com/embedded-pro/e-foc/issues/251)) ([5cfd307](https://github.com/embedded-pro/e-foc/commit/5cfd3070f349164ae7dca473fec6fbaad03081f8))
* Run SIL from an emulator ([#249](https://github.com/embedded-pro/e-foc/issues/249)) ([3e44762](https://github.com/embedded-pro/e-foc/commit/3e447621621422af18f67ffb6459a88fb02de8da))


### Bug Fixes

* **docs:** Copy TikZ SVGs to site and rewrite paths for HTML output ([#254](https://github.com/embedded-pro/e-foc/issues/254)) ([86b4859](https://github.com/embedded-pro/e-foc/commit/86b48597c07f117fd1d501ee4f42fcb25223eba5))
* **docs:** Render TikZ figures to SVG for HTML booklet output ([#253](https://github.com/embedded-pro/e-foc/issues/253)) ([2834596](https://github.com/embedded-pro/e-foc/commit/28345960f5341e03ed8c888dabe48803bca230d0))
* Fix hw configuration ([#74](https://github.com/embedded-pro/e-foc/issues/74)) ([e00421e](https://github.com/embedded-pro/e-foc/commit/e00421e5d530cc9b0410fcd8b156bff6b176c819))
* Implement verified remediation findings from audit ([#236](https://github.com/embedded-pro/e-foc/issues/236)) ([4416c85](https://github.com/embedded-pro/e-foc/commit/4416c85eaeaaa2333c3219a019bcb2b4552042eb))
