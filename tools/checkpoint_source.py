#!/usr/bin/env python3
"""Save an implementation checkpoint without building/publishing a new ROM.

The verified 0.5.43 ROM and its proofs remain explicitly separate. A successful
compile, generated table or source archive never implies gameplay completion.
"""
import hashlib,json,zipfile
from pathlib import Path
from integration_status import apply_current_integration
R=Path(__file__).resolve().parents[1]
def main():
 version=(R/'VERSION').read_text().strip();g=R/'generated'
 status=json.loads((g/'port-status.json').read_text())
 if status['version']=='0.5.43':(g/'checkpoint-0.5.43-status.json').write_text(json.dumps(status,indent=2)+'\n')
 baseline=json.loads((g/'checkpoint-0.5.43-status.json').read_text())
 status=dict(version=version,requested_scope='Full original source port: title, all missions, bosses, results, 1P/2P ending variants',complete=False,
  workflow='Implement full dependency groups first; integrated tests only after full source connection',
  verified_rom_version='0.5.43',executable_scope=baseline['executable_scope'],new_rom_published=False,
  implementation_only=['Typed animation command reader',
   'All 153 upper pose tables and 81 lower/body motion headers compiled, including both characters and all 5 actual weapon columns',
   '52 player animation callbacks and 4 native owner fields translated to C',
   '82 arm selector entry points translated to ordinary C branches, including unnamed $03A824',
   'Original upper-body follow, aim-change scheduling and body/arm completion signaling',
   'Inventory/weapon ammo arithmetic, projectile accounting, aim direction and action-selection priority',
   'Weapon drawing/attack data, animation owner callbacks and casing/blood/blast animation lifetimes',
   'Original projectile angle/spread/vector math, flame curvature, rocket seeking and trail following',
   'Native parent/child scheduler, same-tick insertion, deferred deletion and common pre/post timer/flag rules',
   'Five weapon projectile lifecycles, grenade variants, impact/explosion states and auxiliary effect children',
   '64 complete two-half terrain shapes, slope/fraction profiles, point/fall/body-fall/jump/walking policies and 16-slot moving-platform ring',
   'Explicit world/source coordinate conversion preserves inverted Y fractional position',
   'Complete air aim/action pose selection, jump and fall handler bodies, shared frame/death tails',
   'Nine player defenses, body action probe and melee attack, original input vectors and two 34-entry death routing tables',
   'Ground/crouch native state entries and frame transitions, walking lower-body phase preservation, 73 named body motion bindings',
   'Dropped empty weapon movement, bounce/blink retirement and its original impact child',
   'NativePlayWorld wires body/arm callbacks, projectile emissions, terrain movement and built-in hit targets through one entity scheduler',
   'Explicit weapon inventory pickup entry point plus native world item pickup actors and overlap arbitration',
   'Seven source death routing classes, alternate/smoke selection, falling/landed/timed death, six-tick pit and forty-tick retirement tails',
   'Source spawn-mode fall/parachute/freefall/landing handlers and explicit respawn API with scene-supplied placement',
   'Player death splash/ripple/smoke, player icon and parachute open/swing/release/fall effect lifecycles',
   'Eight-record/four-write target ring with native registration/claim/follow, hanging and attachment jump/landing states',
   'Both ridden-player poses, exact down/jump and all-bits exit-combo priority, scene-gate exit, bounds and death tails',
   'Embedded Slug terrain contacts in source B/A/C order, spring velocities, drop-through/floor following, chassis slope/height and contact-count helpers',
   'Slug heartbeat/ownership, mount-candidate handshake, ammo/fire gates and repair arithmetic',
   'Three Slug damage-display programs compiled to 146 typed commands, with native palette/blink/wait/jump/end handling',
   'Eight Slug motion wrappers, separate terrain/body Y with shared fractions, ground-Y wrapping and body-depth priority, exact input flags and jump policy',
   'Slug body animation owner: 3 typed fields, all 13 graph callbacks, native mount/defense bindings, ephemeral dust/spark children and destruction-bubble lifetime',
   '39 Slug motion tables and 80 two-stage turn selections compiled to 9871 typed animation commands and 955 drawing records; 978 mount records, 47 defense sets and 5 attack sets (9 records)',
   'Reserved ENTITY_SLUG fixed instance, hit registration, regular spawn requests and actual body/contact tick wiring in NativePlayWorld',
   'Hand-written Slug body controller: 95 semantic ground/crouch/slope/jump/fall/landing, hit-reaction, unoccupied/mount, firing/recoil and destruction/retirement entry/frame states, with ordered next-tick overrides and same-tick entry aliases',
   'Eleven extracted Slug body route/steer/sound/bounds tables, source ledger, eight-tick acceleration phases, turn-phase selectors, preserved landing timer and input/time-up/impact tails; former 7 destruction/retirement destinations now implemented',
   'Mount arm overlay: both characters and five weapons, 65 angle offsets, parent follow, completion sound and next-tick deletion; 52 typed commands and 24 drawing records',
   'Ordinary cannon, stationary drop, muzzle exhaust, two-tick trails, three-stage crash attack and smoke states; source parent count release and first/last child ordering',
   'Cannon data: 1036 typed commands, 421 drawing records, ten 32-way angle selectors, five attack sets with ten records, two five-row offset tables and exact byte log/exp angle calculation',
   'Typed hit defense-resistance result for the no-explosion branch; vehicle surface-effect variant and exact foot-contact service; ENTITY_SLUG_AUX scheduler and hit resolution',
   'Slug death launch/explosion, destroyed slide/knocked/slope dash, timed self-destruct attack and forty-tick retirement; immediate cleanup followed by next-tick fixed-slot reset',
   'Explosion/smoke, four-tick debris spawner, all five reachable debris kinds with sixteen weighted choices and self-destruct blast child; 291 typed animation commands and 69 drawing records',
   'Four palette records separated from their count, original three-allocation limit, release order and damage-display reset; explicit renderer-provided debris sprite-budget admission',
   'Source owner and damage-attribution copying for cannon, crash attack and self-destruct blast children',
   'Five turret variants, 80 aiming selections, 192 drawing selections, burst fire, terrain muzzle spark, detach/wait/restore-source branch, thrown part/fire, wheel and mount marker; 238 typed commands and 186 drawings',
   'Slug and POW water/mud followers with flash, attachment, delay-table cycling and detached effects; 216 commands, 87 drawings and shared twelve delay-table rows',
   'Ordinary spawn wait/discard policies, five clamped variants, init/drop/descend/landing handlers and wheel-marker-turret-contact-surface order',
   'Source fixed-slot execution order and player status/crouch-trigger reset hooks; opposite-phase mask clearing without invented nibble shifting',
   'Correct $3B Animation.pose binding, pre-first-tick contact sentinels, initial turret shot/spread bytes and fourth palette-record metadata alias',
   'Boss-specific arena constructor, both drop positions/palettes, descend and steady/landing paths: six added semantic body states and explicit fixed-slot API',
   'World item actors: four weapons, eleven cycling sequences, three supply kinds, twenty ordinary foods and timed/rot/thrown food; 31 kinds with executable C handlers',
   'Source pickup overlap, player query gate, unsigned X arbitration, invulnerability delay, weapon flash and mounted sound, ammo/grenades/Slug repair with score fallbacks',
   'Food lifetime/byte blink, timed animation changes, rot, thrown/bounce movement, combo chain with signed-byte comparisons and scheduled combo controller',
   'Table/value score popup parents, 32 glyph selections, per-digit delayed ballistics/lift/hold/blink and deferred/recursive cleanup; BCD score addition and cap',
   'Counted fly-away/resume wrapper, frame-boundary item count rollover, direct template dispatch and parent-offset/below/thrown spawn helpers',
   'Item animation data: 886 typed commands, 283 drawing records, four owner fields, two overlapping score/count tables, four notices and original templates',
   'Dedicated WMOVE_ITEM gravity/contact wrapper, common entity/hit resolution and NativePlayWorld item/score storage',
   'Four actual POW entries and 28 semantic states: bound/stacked/idle/free, release and cheer children, walking/climb/descent/crouch-hop, rescue credit/gift and exit',
   'Breakable item crate 293: animation-driven defense, hit/death/drop, moving-platform publication and body contact with players and Slug',
   'Two eight-record climb buffers, source nearest/crossing/snap helpers, source WORD ground-test wrapper and template 143 edge publisher',
   'Stationary/attached gun 283: target ring publication, three-owner overlay, aim acceleration, player burst/bullets, original enemy cooldown and rank-selected shell attack',
   'Gun explosion lifetime and explicit scene cleanup API; six body-contact descriptors and player contact-state tracking',
   'Interaction data: 609 typed commands, 273 drawings, eight defense sets, four callbacks, rank/gun/contact tables and raw sibling drawing conversion',
   'ENTITY_INTERACTION resolves through common scheduler/hit targets; NativePlayWorld owns rescue counts, rank inputs and climb buffers',
   'Infantry templates 0/27/28/249/282: both guards, charger, skirmisher and their reachable escape states; original target turn, timers, melee probe/attack, five throw selections, windups and hit/death/bounds tails',
   'Four enemy ordnance families: 24-trajectory grenade with four scene modes, bouncing timed bomb, rank-HP mortar shell and terrain-following roller; collision/damage/explosion and deferred/recursive retirement',
   'Four human death routing tables with 34 damage entries each: tumble/repeat/knockdown, collapse, randomized burst/charred body, scaling launch, horizontal/vertical shotgun impact, burning/run/turn, fade/blink, blood/flame/smoke children and corpse constructors',
    'All 24 callbacks and 7 owner fields in the enemy animation group translated; 3008 commands, 840 drawing records, 18 defense and 21 attack records are data counts, not completed-feature counts',
   'Source nearest-target distance and ties, P1 self-Y quirk, filtered nearest-X and byte angle helpers; screen-word eight-record/four-write death position ring',
   'Both direct and weighted-random rank table formats for infantry; explicit rank and player-count inputs, preserved RNG ordering',
   'Original per-hit score dispatch and score-owner attribution integrated before optional external damage observer, preserving renderer visibility gate',
   'Ordinary soldier templates 1 through 22: 56 semantic entry/frame states for full Think, movement, crouch, melee, throws, jump/fall/landing, taunt and escape, plus two jump-in child constructors',
   'All three Slug enemy anchors: available-slot selection, leap/align, claim/refresh, input struggle, damage, cannon-detach signal and release; distinct foot-player tackle and four trajectories',
   'Soldier climb-nearest/crossing/up/down/attack/dismount transitions and source attack-exclusion zones: template 152 and eight-record/four-write frame ring',
   'Original no-slope-transition ground movement, complete escape Think and separate source unrotated/bounded-rotated RNG transitions; existing mission source/data join the compile group',
    'Allen boss template 328: approach/intro, ordered target/turn/taunt/MG/knife/jump decisions, landing, capped per-frame damage, health-stage private palettes, ground/air death and retirement',
    'Four Allen defense sensors, both muzzle-flash followers, rank-speed level/diagonal bullets, aimed grenade and three explosion entry paths, attack callbacks and shared human blood children',
    'Allen preserves secondary-view Y, source substep AND condition, separate invulnerability/fade timers and first-child scheduling; no inferred health HUD or unreachable bullet-impact branch was added to combat behavior',
    'M2 boss 132: intro and destruction callbacks, moving support/body/turret, ranked HP/shell speed/cooldown, seven-sector aim/recoil, smoke and shell effects, death swing/fall, scene flags and deferred retirement',
    'Eight original M2 background tile-block patches converted with persistent destruction mask; nine wreck and nine burst callbacks implemented, renderer application still pending',
    'Crab 138/139/140: activation, charge/pause/return and patrol, two claws and three legs, source pose-driven extension, detached defense/attack chains, bullet knockback, fragments and death/surface effects',
    'Gunship 75: eight authored path selections and 76 bounded typed path commands, crew and detached low/high weapon movement, dive/crash/destruction, wreck pieces and scripted arc constructor',
    'Shared follow/timed/burst effect emitters, eleven debris choices, metal/wreck ballistics, three/five-sprite surface effects, damage-threshold smoke controller and drifting fire/smoke puffs',
    'Shared boss animation group now includes crash fragments and M5/turret/paired-body effect patterns: 2009 commands, 490 drawings and 33 callbacks; converted counts are not coverage or gameplay proof',
    'Generic source body-contact descriptor consumption, explicit parent-generation references, signed path rounding and original delay-table reads retained; no executable source CPU pointers',
    'M5 111/112/113: body entrance/hover/three attack loops/barrage, ranked timing and HP thresholds, rotor/parts, attached bomb flight/homing/bounce/shell modes, sparks, rockets and death/scene signals',
    'M5 soldiers/grenadier: walk/acquire/turn/throw, crew arrival/raising, aimed grenade sequence, hit/death handoff, body-death ejection/settle/rest and scripted player-lock release',
    'M5 attached turret: body/driver, pose/turn/recoil, history-delayed homing cannon, shell trails, child teardown and original infantry escape handoff; separate base/loot variants remain pending',
    'M5 arena gate/deck/three sections/lights/nose/hatches: ordered camera relink outputs, platform-ring publication, open/close/damage shutdown and converted nose terrain descriptor; camera/map consumers remain pending',
    'M5 data: 4067 animation commands, 768 drawings, 20 callback IDs, nine fields, 50 flight commands/eight path selections and 24 rank tables, all compiled offline with reviewed selector bounds',
    'Paired body boss 114/115/116: root heartbeat and end gate, attached/independent left/right constructors, moving/turn/wait/resume/death states and two ordered part-attack clocks with one-body phase changes',
    'Paired body parts: five limb variants and immediate command dispatch, head proximity/hit/death/flash/respawn/retirement, cannon/beam/flame followers and ballistic projectiles, hit score and corpse-controller adoption',
    'Paired body data: 1024 animation commands, 302 drawings, 11 callbacks, two fields and 13 rank tables; the source label FinalBoss is not proof of in-game identity',
    'Common damage-smoke body variant, 16 crash-fragment bursts, 17 heavy-metal death fragments, preserved body palette metadata and original parent/child scheduling',
    'Tank 118/119: drive/turn/retreat, damage-threshold and minimum-life phases, hull/cabin/launcher/gun/turret, destroyed turret/POW release, body destruction and scene-lock release',
    'Tank projectiles: 16 authored paths/140 typed path commands, fixed-target homing and delayed gravity, continuous muzzle/flame stages, ballistic fragments and palette-follow smoke',
    'Tank attached bazooka crew: ordered command/acknowledgement control, 16-angle aiming/firing, rocket/muzzle children, hurt/death/fade and detached weapon; standalone vehicle-ejection family connected in0.5.70',
    'Both tank watched reinforcement scripts translated with threshold/period/count/enemy gates, original one-choice RNG consumption, cancellation of descendant spawners and real template 228 dispatch',
    'Reinforcement aliases 228/229: settle/entry/move/landing/turn/random wait, item drop and existing human/smoke death handoff',
    'Tank data: 1821 animation commands, 510 drawings, 11 callbacks, ten fields and 19 rank tables, including source weighted-choice indexing; data counts are not coverage',
    'Shared collision-stamp writer preserves column-major order and original clipped-column payload behavior; requires a real mutable map service',
    'POW constructor carries the original animation-freeze values 0/4/8; corrected the earlier $59 damage-timer binding to $44 animation.freeze',
    'Carrier 129/130/131: approach/braking/cannon clocks, three destructible hulls, moving platform/contact publication, staged cockpit separation and hatch destruction',
    'Carrier crew: three occupied spawn lanes, rank-based rate/attack cooldowns, grenade throws, hit/death slot release, replacement gunners, jump/dive/land/retirement, and both attached guard variants',
    'Carrier data: 812 typed commands, 242 drawing records, three executable callbacks, six fields and eight rank tables; source RNG consumption and unsigned arc-table indexing preserved',
    'Template 117 body: approach guard, target/drive/brake/bob/upgrade/death and six independent plates with ordered damage relay',
    'Template 117 weapons: central seven-way tracking/beam/recoil, both side open/fire/countdown/burst/close/death loops, attached attacks and ten troop children',
    'Template 117 effects: projectile knockback/fall/impact, scatter and ballistic/ground effects, paired blasts, scheduled destruction patterns, common selected debris and scene finale signals',
    'Template 117 data: 807 commands, 236 drawings, 12 executable callbacks, two fields and 18 rank tables; original double timers, parent-written scatter index and word hit-kind comparison retained',
    'Common bounce-blast now uses source WMOVE_GROUND; typed palette-B blast and caller-selected scatter physics are shared with the new boss group',
    'Source scene-three siege: gates, staged column damage, tile/terrain destruction, reactive props, eight/nine-part rows, guard lists and score/POW/reward interactions',
    'Siege finale: 45 three-stage panel children, four auxiliary guards, nine sequential collapse steps, body/ally bazooka/crew/parachute/escape and final close/player-icon signals',
    'Templates 56/57/58/253 and guards 31..39: gated entries, RNG/rank delays, throw/turn/reaction/hurt/death/replacement and projectile palette/angle/attack lifetimes',
    'Sixteen original scene-three placement records feed a typed constructor using explicit secondary-camera coordinates; original scene scheduler remains pending',
    'Siege data: 1062 commands, 177 drawings, ten executable callbacks, 154 offline-cropped tile patches and seven terrain stamps; guard data: 703 commands, 125 drawings and one callback',
    'Shared source masked RNG, concrete ordered SceneVideo tile writes and palette-cycle request, palette 82 fire/smoke, AD/77 debris, direct launched-corpse entry and standalone ally bazooka controller',
    'All 72 mission streams: 2991 commands, 14 scene-slot selectors and regional final alternative; camera-table island excluded from executable records',
    'Native ENTITY_MISSION: first-child periodic events, typed parent-watch cancellation, task thresholds/delays/repeats/enemy gates and shared RNG; two tank watches use the common scheduler',
    'Mission spawn conversion uses explicit secondary camera; counted enemy flags, immediate counts and delete-time clearing are connected; op 7 retains pause/latch and compares the low byte',
    'Fortress roots 0..4 with independently damaged pills/crates and parent HP propagation, seven hatches with normal/final watched events, thirteen wall pieces, props/sign and quad group',
    'Fortress armored car/turret/staged destruction, helicopter/rotor/staged crash/attached climb publisher, barricade child drain, blast/debris patterns and native template 269',
    'Seventeen scene-four placements, 137 cropped tile patches, ten terrain stamps and 75 executable animation callbacks; 1632 animation commands and 217 drawings',
    'Arrival templates 40..44 transfer to ordinary soldier 1/11 or forced escape; guard graph grows to 814 commands/157 drawings; source 077228 climb publisher has typed parent retirement',
    'Scene-five airship: enter/land/hover/terrain recoil/depart, normal/special platforms, camera-lock/scroll outputs, ground/players/water and crouch-trigger children, actual attached gun call',
    'Separate source airship gun: target-ring registration/owner overlay/oscillating aim, source-parented bullets, casing bounces/freeze, owner death cooldown and release',
    'Both scene-five towers: ports/ladder/watched reinforcement, source hit-kind gates, rank HP and 666/333 stages, scene-12 direct destruction, tile/terrain patches and palette-cycle callbacks',
    'Scene-five camp/tents/depot two-frame attack/event, bunker 258, five crates/two solids/three props, barrel chain/rock rain, throws 270/drops 254..257 and full local collapse/ricochet effects',
    'Eleven scene-five placement constructors; airship data 911 commands/245 drawings/four callbacks/33 tile patches/15 stamps; actual scene scheduler and consumers remain pending',
    'Stationary guards 168..170: optional ground settle, idle/taunt loops, water follower, preserved hit RNG and death/retirement; common enemy graph 3067 commands/850 drawings',
    'Ambient/NPC 174..179 and 195..226: 38 native templates including fortress reinforcements 175/177/179/202; ground/fall resume, first-hit immunity, directional stagger, dropped prop, sit/hop and proximity animation loops',
    'Arrival 45..49 preserve incoming facing; crouch triggers 323/324 both directions/players; Slug bounds gates 325/326 execute as scheduled actors',
    'M4 268 body/approach/hurt/death/wreck, attached turret/contact/recoil/fall, wheel/mount, ranked 104 dropper and watched destruction event; 263 camera floor, 264 water/splashes, 265 debris and twenty 266 rail children',
    'Free actors 104/105: walk/fall/land, jump/wait, both fire/repeat loops, directional callbacks, projectiles/impact, hat/flash, human death and infantry escape handoff',
    'Mob data 424 commands/162 drawings; M4/free data 493 commands/129 drawings/six callbacks, 22 tile patches and a collision stamp; source counts are not gameplay completion',
    'Shield 108/109: independent shield/body hit propagation, idle/walk/back/guard/fall, crouch and melee repeats, detach/bounce/fade, item drops, human death/escape, muzzle/bullets',
    'M4 lift 267: moving platform/altitude/stop/hold/wobble, progress-gated actual shield109 creation; rescue275: three defenses, HP, original-count randomized actual POW children',
    'Reinforcement 94/95: count/camera gates, child heartbeat/stop, targeted entry, two firing loops, hit/death/leave, projectiles/smoke/splash; barrel141: wait/rise/fall/fire/sink, hit/slide/fragment/floating-mine lifetimes',
    'Light tanks53/54/55: body drive/reverse/brake/idle/resume/aim/fire/hit/wreck, separately damaged turret, attached driver/ejection/ordinary-soldier handoff/corpse, missiles/ballistic shells/trails, 17 heavy debris, drops and two terrain stamps',
    'Shield/shore/light-tank graphs total1905commands/486drawings/19callbacks; ten additional template dispatches are not full-game completion',
    'Armed soldiers96..103: eight original variants, distance/target/facing decisions, walk/run/back/retreat/turn/fall/land, angular aim/fire/repeat/rotate-back/recover and riding-player low attack',
    'Airborne106/107: source trig descent, angular aim/fire, landing-to96 controller, independent parachute HP/animation/hit-to-parent/death; runner110 and front attack cart continue independently after parent hit',
    'All96..110 template entry points dispatch after this batch; eleven added entries are not gameplay completion; shared M4/free data1229commands/276drawings/sixcallbacks, four bounded selectors',
    'Bazooka64/65/86/87/90..93: standalone A/B movement/target/command/ack/repeats/recovery/death/ally-spawner; weapon adaptive range uses stored target, group phase FF and recursive retirement preserved',
    'Gun platforms133..136: body phase protocol, independent hatch/shield/rider/gun controllers, HP and directional human-death handoff, repeated projectiles, five terrain stamps, damage sparks and drops',
    'Ground nest50..52: drive/reverse/brake/avoid/aim/repeated-fire/hit/wreck, crew escape and independent death, rockets/pieces/flash/smoke/shared shells, ranked random delays, two terrain stamps and drops',
    'Fifteen additional template constructors; gun-platform343commands/91drawings/5callbacks and ground-nest689commands/160drawings/9callbacks; ARM127C+one assembly compile only',
    'Rocket vehicles88/89: three independently scheduled terrain contacts, shared chassis helpers, drive/turn/stop/idle/aim/repeat/retirement, live slope states, rider command/acknowledgement and detached fall, trajectory/angle rocket and muzzle children',
    'Attached bazooka crew ejection now hands off after the old weapon receives command7; parent deletion retains original crew death override; source contact order and word-wrapped chassis midpoint preserved',
    'Walkers66..74: nine interleaved constructor variants across three activation/repeat modes, body contact/wake/wait/fire/hit/death, ranked ballistic shell/trail/impact and two terrain stamps; six animation callbacks execute',
    'Eleven added constructors; rocket-vehicle3833commands/777drawings/fourcallbacks/43boundedselectors and walker226commands/73drawings/sixcallbacks; shared impact patterns, flash and second fragment graph are source-only data/executable additions',
    'ENTITY_TERRAIN_CONTACT owns source contact pre/post lifecycle in the common scheduler; static pool/world limits still compile; ARM131C+one assembly,105headers,1524640 group bytes is not ROM or gameplay validation',
    'Mortar60/61: moving/idle/damage-stage/death/wreck/explode body, four child layouts including attached bazooka variant0, independently hit throwing crew and ordinary-soldier handoff, two published surfaces and four death terrain stamps',
    'Cannon62/63: activation/open/target-side turn/repeat/HP-stage/destruction, 25-angle shell selector, alternating projectile palette, muzzle and sparks/explosion patterns with ordered hit/death/outside overrides',
    'Crates122..124: next-tick activation, hit/wreck, four terrain stamps and explicit stamp-free variant; seven added constructors, mortar graph509commands/163drawings/10callbacks/sevenranks, ARM133C+one assembly compile only',
    'Captors125/126: delayed settle/idle/any-hit/death graph/timed fade, blood palette variant and explicit bounds;127/128: three first-child actors, command callbacks/action acknowledgements, human death/escape and typed enclosing-owner completion',
    'Captor-related30: wait/open/react/human-hit/escape and surface effect, independently damaged door with parent notifications/blast/removal, exact next-tick setup and ordered overrides; center-turn alias implemented with no established incoming normal path',
    'Captor graph490commands/112drawings/onecallback/sevendefensebindings; five added constructors,135C+one assembly,107headers, compilation only; enclosing compound-owner behavior still pending',
    '120/121: normal intro/flight/dash/bomb/missile/burst/death and scripted paths, eight ordered part commands/ack/writeback/aim/hurt, ballistic/immune/alternate shells, bombs, guided missiles, ground effects, wreck/blood/radial blasts and scene flags',
    '120 source-only group:1322commands/414drawings/ninecallbacks/39ranktables/12flightcommands; alternate shell0420E4/042188 alias implemented without established incoming graph;040554 ambient-D5 steering branch explicitly unsupported when intro-open X>272',
    'Shared effect allocation/schedule validation now accepts already implemented BFX_CRASH_FRAGMENT_B and BFX_FLASH, fixing the earlier upper-bound omission',
    '137 air formation: flypast/turn/zoom, eight persistent HP slots and spawn/idle relaunch, authored approach/combat routes, pilot launch/attack/palette and overlay command following, six death choices, terrain/water/split/crash, scaled radial blasts and shard/debris patterns',
    '137 graph624commands/164drawings/one animation callback/10ranktables;22pathentries/267commands/nine path callbacks; additional source aliases implemented but unreferenced are separately recorded;22 authored mission spawn records inspected, not run',
    'Source allocator selector FFFF restored in120 and137; full parent follow preserves child display group; pilot palette uses source first/second/first/third four-frame sequence',
    '76..85: eight body variants and scheduled shared count/stop control, wait/drive/brake/charge/run/leap/throw/death, rider command/ack/aim/fire/eject, attached overlays/trails, guided missiles/bombs/debris/muzzles/surface lifetimes and ordered spawn tables',
    '76..85 graph877commands/267drawings/12nativecallbacks/11ranktables; source aliases64FF8/651E6/65668/65CE8/65E6E have explicit APIs but no established incoming normal graph;65F24 next-task depth/child-presence helper implemented in0.5.76',
    'Shared77F46 trail uses its original graph/palette/bounds;4A7A6 charred child uses a ground probe and charred body rather than the unrelated smoke-body alias',
    '59: attached heartbeat/repeat emitter, combat enter/open/turn/aim/fire/release/fall/land/death, ballistic shells/muzzles/exhaust/puffs,17-piece breakup and terrain stamp;804commands/192drawings/eight callbacks/10ranks',
    '59 released-actor selection063106 advances RNG but uses incoming D1; explicit typed selector API exists, automatic selector unresolved and absence faults; not a complete59 port',
    'Static constructor inventory:241 of329 IDs dispatch,88 unwired;164 of222 authored mission IDs dispatch and58 are missing. Constructor counts are not behavior or released-game coverage',
    '065F24 source helper is next-task depth comparison, now mapped to child presence; corrected prior age/previous-task interpretation. Shared selected scatter graphs preserve source <=0 speed default512',
    '142 airborne turret body target/steer/orbit/seven poses/hurt palettes/burst/gates; seven16-frame manual gun streams, barrels, shells, crash attack, two destruction paths, drops and exit',
    '144..150 three obstacle families: two-stage HP, hit/collapse/wreck, death-position publication,21 converted terrain stamps with276..280 static props; source allocation-only aliases remain distinct',
    '23..26 release devices with activation/open/fade,33-point crew trajectory, exact5783E/57880 soldier handoff and falling debris;514commands/185drawings/five callbacks/nine ranks across the two new groups',
    '17 new constructors:258 of329 dispatches;171 of222 authored mission IDs dispatch,51 missing. These are dispatch counts, not behavior completion or gameplay validation',
    'Shared78704 palette28 fragment and two weighted destruction patterns added;59 soldier handoff corrected to04498E global100800 projectile group preserving task order',
    '153..155,157..167,180..184: camera-gated static/carrier release and child, random stand/walk, staged hit/launch/bob, fixed/directional run, sentry pulse/repeat, swing/pause/back/turn, bobber/leaper;19 constructors and six explicit unused fixed wrappers',
    '292commands/93drawings and one animation signal field; exact unsigned secondary-scroll trigger, one-sided fixed-player test, original accumulating pause timer, direct8018 priority and advance-before-acceleration integration',
    'Static constructor inventory now277 of329 IDs,183 of222 authored IDs dispatch,39 authored IDs missing;156 capsule/mission-end and185..191 dependency groups still pending',
    '185..191: four-hit nest and parent-triggered fall, one-frame swarm pulse, parent/random/flee/wander motion, static/tertiary-camera followers, lob/land and three shard variants;188commands/55drawings',
    '156/244..248/320 capsule: settle/descend/open/flash/scale pulses/rise/out/dim/fall/retirement, marker/scroll/hold, final flash and real189/dropper children, timed fade and scene playback/variant outputs;249commands/77drawings',
    'Global RGB fade preserves four source rate entries and different up/down first-frame timing; raw audio control command0A/value80 is typed, source5C saved-word/high-byte signal alias preserved',
    'No-SYNC screen position compensation and independent tertiary-camera deltas, plus142 pre-SYNC target/steering queries; scene B/C driver, camera/fade/audio consumers still pending',
    'Static constructor inventory now291 of329 dispatches,195 of222 authored IDs,27 authored missing;14 added constructors do not mean gameplay completion',
    '230..243 static/random/table-palette scaled banners and thrown sprites, with actual723D2 parent-HP smoke;321/322 switch existing heavy-grenade collision and327 publishes continue count',
    '259..262 roof hit/blast/child drain, signs with terrain stamps, boat rank/contact/platform/HP/hit/destruction/area blast and rare fragment;250 bounded record maximum,252 audio-reply heading,319 manually steered platform',
    '272/273/274 dispatch existing60764/60A0E/4A802 aliases; unused frog ground/jump/run entry also implemented; four common destruction FX patterns added',
    '271 flame trap orders target groups, overlay, cooldown, two scheduled relay stages, signed invulnerability check/player velocity change and follow particles; source allocation and task order preserved',
    '573commands/170drawings across three new graphs; sound reply106F29 is explicit input, source45 boat invulnerability corrected and original4F9C4 overwritten vertical calculation preserved',
    '28 constructor additions:319/329 dispatches,219/222 authored IDs,3 authored missing29/151/194; remaining10 total entries and full scene/render/audio/HUD/results/endings/2P are not complete',
    '29 standalone turret shares M5 body/driver/guided cannon while preserving activation, sync/collision/retirement modes, bounded16 initial RNG, signed timer and post-death schedule overrides; compound phase88 patch extracted',
    '171/193 automatic and steerable attachment guns: registration/claim/release, input/AI aim, fire/reload, overlay map indices and projectile attack/defense/impact;193 literal ROM4000 WORD priority and explicit owner0 preserved',
    '194 walking/falling/attachment claimant and human hit/death; target-ring renewal now returns after matching record update;281 preserves incoming facing;317 settles/stamps44x12 terrain/animates/culls',
    'Six new constructors:325/329 dispatches,221/222 authored IDs; only authored missing151; total missing151/227/251/318.240 additional commands/104drawings,compile only',
    '151 compound structure:82 typed mini-script commands,14tile patches,clear/stamp,palette remap,root/parts/doors/lowHP/towers/flame/smoke/projectiles/trails/collapse and scene events/unlock;75EFA original idle boot-parentFFFF context is connected,2B58 palette-table-tail read preserved',
    '227 fixed tile panels and5 blinking prompts,logo alias;251four ordered automatic input handlers with body/Slug point-of-use selection;318fall/bounce/flat/recover signal with unresolved inherited render geometry explicitly required',
    '329/329 constructor dispatches and222/222 authored IDs wired;660 new graph commands/190drawings; constructor presence is NOT completed behavior',
    'All16 scene entries:855 commands/191inline callbacks;4plane loader/parallax/edge copies/column controls/commit/hide/clear,53tile images/19collision images,mutable4096byte collision map and native terrain wiring',
    'Exact scene camera follow/clamp/progress/smoothing/seed snapshot;continuous native background origin;216spawn positions with source modes;M5 relink/wrap/nose/scroll,airship scroll,M4 floor,capsule seeds,M2 patches consumed;32byte source audio queue/filter',
    'Eight separate static placement lists145records:113 connected/32 explicitly pending; signed progress trigger,FIRST/global group/kind2,ending12bit X,source11FF; shared siege/fort/airship APIs route common dispatcher',
    'Village destruction: wall/sign/large/ladder/explosive/houses/huts/tower watch/gate/swing+auxplane child/markers,static/lamp/sensor and ballistic FX;906commands141drawings3callbacks44tilepatches14patterns; source-only ARM190C+oneasm',
    'Opening compound placements:hut/tower/bunker/bridge/nest/shed,barrier-gate-fort chain,19 independent parts,watch/contact/tiles/stamps and block+puller crew with falling attack/platform/land/escape/death;541commands103drawings2callbacks76patches23patterns11stamps',
    'Static placements now126/145 dispatch,19urban records remain; source51DE2 over-width atlas reads,pointer-high-word margin41,blank nuke allocator and delayed crew escape adoption preserved',
    'Urban16constructors/19records:compound wall/neon/captor127,building/column/stall/fragile/hit-stage/small/ranked/5multistage/blocker;1450commands119drawings163callbacks170patches29patterns12stamps,all145 static placements dispatch',
    'Six9E debris+AD alias,blink,actor-private palette-cycle output;46608 whiteflash/1081B1 latch and52580 signed8.8 RGB task.5DD22 preserves priority,2813C fixed mapY;59 source62 byte does not replace98 params',
    'Seven regular scene drivers and endingB/C/D:playerfollow,mapframe,cameraX/progress placement distinction,capsule and six speed thresholds; once-per-frame NativePlayWorld controller wiring',
    'Panel/banner384word2x2text,audio-reply reveal and next-row redraw,3GO timelines54records,watcher andcontinued label; mission start spawns watcher+automatic-input driver,debugFIX/raw-input clear aliases',
    '106E88 wait consumesprevious counted enemies,notcampaign timer; attached scene unifiesactor audio queue,2222prefix0C,raw0A80 and2242 one-per-frame reply decoder',
    'Final result593FA..59BD2:sequential typewriter/wipe/P1P2 panels/RNG count rolls/8digit score,4orbit sprites,fade,parent completion andpeace alias; ResultWorld wired with explicit snapshot validity',
    'Highscore defaults10,97AAA signedpacked insertion/tie ranks,41glyph3letter input,raw edges203/209 andrepeat204/20A,300/900timeouts,censor/commit/96blink; real children inresult panels andstandalone table mode',
    '595AE count semantics and97ABC no-op codebyte preserved;59A1A incomingD0.low1 resolvedfromunique15B6/15F4 parent45=2 scheduler decrement,emits0A21.11strings11suffixes41chars10defaults8palettes2drawings',
    'Regular mission results3DBC8..3EE3A: columns,START accelerated timing,prizeflight/7row weighted roster10786bytes,POW/vehicle/winner scoring,zero/tie/winner labels,fade andcomplete banner',
    '7A970/7A9F0 START/COMPLETE plusFINAL:four layouts70letters,175commands53drawings,seek/scale/palette/ack/blink/exit/fade/lock clear;ResultWorld live state/raw214/21A/snapshot APIs,private palette flags',
    'ENTITY_ENEMY common scheduler/hit resolution, exact synchronized ground status helpers, source owner sentinels and shared play_world_spawn_template for implemented native families'],
  not_connected=['Full player drawing renderer and released main/game.c loop',
   'Physical main/input integration of NativeSession; special/debug scene selection beyond regular campaign',
   'Physical boss drawing and full gameplay validation of the connected defeat/result/next-mission transitions',
   'Renderer sprite-pool counters for debris admission, physical palette handles and source visual metadata',
   'Original BIOS-specific credit banks and physical frontend/ranking; shared credits/LEVEL/start eligibility are implemented',
   'Remaining enemy hit-target aliases, physical scene/audio backends, video job queue and renderer visibility inputs',
   'Original item/POW/gun placement/event callers, parent-selected climb alias and remaining alternate entries',
   'Scene-coordinate item offset selector, explicit-value popup aliases with parent-field writes, item/digit rendering and HUD'],
  pending=['Finish complete player action/weapon/graphics dependency group','Remaining original enemy/POW/vehicle state families and unconnected alternate entry aliases',
   'Full source comparison and physical main connection of NativeSession; seven regular and three ending drivers plus map/script/camera core are connected','All mission flow consumers; remaining boss/vehicle groups and aliases, all329 constructors dispatch, with59 D1/120 D5 exceptional inputs and318 original28C20 inherited renderer A3/A4 still unresolved; see native-template-inventory.json','Title/attract/tutorial/ranking/return-to-title; NativeSession now calls regular/final results,highscore,gameover and BCD endings',
   'Integrated original-source and ARM gameplay/performance tests after complete implementation'],
  tests_run_during_implementation=False,compilation=json.loads((g/'port-compile.json').read_text()),
  source_primary=baseline['source_primary'],commit=baseline['commit'])
 status['implementation_only'].extend([
  'NativeSession regular scene order0,1,3,2,4,5; boot/player/scene scheduler order; boss lock and transition hold -> regular results -> snapshot/cleanup/next mission -> final results -> endingB/C/D -> gameover/RETURN_TITLE',
  'Player controls2575C/25766: join/lives/death-wait/respawn/continue/countdown/scene item/highscore; explicit GBA shared-credit policy; raw start/edge/repeat separated',
  'Unified106E92 mission_time shared by250/body/Slug/HUD; source90-frame warmup and frame-bit clock/TIMEUP; original HUD lives/score/ammo/grenade/POW/Slug dampedHP bar',
  'Session data30animation commands10drawings,9strings,2UP streams,60scale bytes,49palette records;214C+oneasm155headers compilation only'])
 status['session']=dict(source='generated/session-source.json',entry='native_session_begin/native_session_frame',scene_order=[0,1,3,2,4,5],
  completion='SESSION_RETURN_TITLE handoff; original title return still pending',
  credit_policy='Explicit GBA shared credits/freeplay; not arcade/home BIOS bank emulation',
  time_state='EnemyWorld.mission_time; one persistent106E92 word',physical_main_connected=False,gameplay_verified=False,
  remaining=['Physical render/audio/main and hardware input/IRQ invocation','Video job queue/renderer budgets and106F4B consumer','Title/attract/tutorial/return','Special/debug scene selection','59D1/120D5/318A3A4','2P link','Integrated tests after full source connection'])
 status['implementation_only'].extend([
  '0.5.91 SourceInput25066/5CC1A edge/repeat/raw/active/system; two original demo streams with delay/toggle/end and513th-byte lookahead',
  '799A4 difficulty/demo/Slug presence sampled at use;35 consumer source files and273 table-owner entries preserve2AC4C table+6A->actor68 side effect',
  '1B1C/1B40 preframe live-body count;448A6 player-count/Slug gates, renamed misleading mission difficulty fields',
  'Boot44 action freeze distinct from45 join gate;1081AE grenade collision reset;106F2B=3; source frame phase increments before actors',
  '13C3C whole-frame pause with blink/sounds;10E274 is pause permission, not visibility;layout2 single-body death requests30 VBlank hold',
  '52050 credits/5223A LEVEL flags and regional positions,52328 eligibility; explicit GBA shared balance policy',
  'NativeRuntime serialized VBlank audio/hold/cadence/ready and input/pause/session/prepare; real callbacks required, no physical main or IRQ hookup',
  '226A one audio byte per VBlank, real reply input, force0F bypass; no fake audio events or playback backend',
  '218C+oneasm158headers ARM compilation/group link only; no ROM or gameplay tests'])
 status['runtime']=dict(source='generated/runtime-source.json',entry='native_runtime_init/native_runtime_vblank/native_runtime_pump',
  rank_consumer_files=35,rank_owner_table_entries=273,input_producer='source_input_frame; raw and active snapshots',
  physical_main_connected=False,hardware_backends_implemented=False,gameplay_verified=False,
  threading='Serialized caller; hardware IRQ/critical sections pending',
  remaining=['Renderer/OAM/VRAM/palette/assets and staged video jobs','Real audio playback/replies','Hardware main/input integration',
   'Title/attract/tutorial/ranking/return','106F4B consumer','59D1/120D5/318A3A4','2P link','Integrated tests after full implementation'])
 status['implementation_only'].extend([
  '0.5.92 all54 program drawing ledgers bound to live Animation drawing_program identity;13081 drawings/13219 parts/9367 maps/18822 pieces/80107 tile cells',
  'AN_DRAW publishes program+ID; animation_begin preserves previous drawing identity;13 manual drawing sites assign their owning program explicitly',
  'Full fingerprinted C1..C4 andS1 converted losslessly to GBA4bpp:131072 sprite tiles and4096 FIX tiles;1536 original RGB555 palettes',
  'native_drawing_resolve/visit expose reviewed pieces and tile columns to the future physical renderer; source271B44 distant x944 tile0 preserved',
  '220C+2asm160headers compile/group only;20,138,364 group bytes include graphics ROM, not RAM or final cartridge;EntityPool63504 bytes'])
 status['graphics']=dict(source='generated/drawing-source.json',resolver='native_drawing_resolve/native_drawing_visit',
  programs=54,drawings=13081,maps=9367,sprite_tiles=131072,fixed_tiles=4096,palettes=1536,
  pixel_conversion='Lossless source indices, nativeGBA4bpp, unscaled; final approved75percent composition pending',
  physical_renderer_complete=False,gameplay_verified=False,
  remaining=['Native object render queue/sort/priority/zoom/palette/water reflection','OAM/VRAM cache and limits',
   'Background/FIX/commit/prepare and main wiring','Final memory/layout/performance verification after integration'])
 scaled=json.loads((g/'scaled-drawing-source.json').read_text())
 status['implementation_only'].extend([
  '0.5.93 source320x224 ->75percent240x168 -> viewport240x160; centre crop4top/4bottom; original physics unchanged',
  'All9367 maps offline composed at75percent;22763 legalOBJ chunks,20783 unique blocks,7022624 bytes; exact indexed-colour packing roundtrip',
  'All131072 source tiles ->12x12 and4096 FIX tiles ->6x6 offline intermediates; fullBG/FIX compositor remains',
  'Scaled drawing resolver, basic clipping/flips/stable caller-key sort/frame-local tile+palette reuse/OAM staging; explicit128OBJ/16palette/1024tile caps',
  'Actual serialized DMA3 OBJ tile/palette/OAM transfer function; blank frames avoid zero-count DMA; no IRQ/main invocation yet',
  '224C+3asm164headers compile/group27512984 bytes only; noROM/gameplay/performance tests'])
 status['graphics']['pixel_conversion']='Original source retained; all referenced maps offline75percent packed forOBJ; fullBG/FIX intermediate assets generated'
 status['graphics']['remaining']=['Source placement/priority/palette/private-fade resolver; dynamiczoom; water reflection; visibility/budget outputs',
  'Approved hand-repaired player pixels binding into all-pose renderer',
  'Full offlineBG/FIX composition and physicalHUD; video jobs/main/audio integration',
  'Final memory/layout/performance verification after full integration']
 status['graphics']['scaled']=dict(source='generated/scaled-drawing-source.json',maps=scaled['maps'],chunks=scaled['chunks'],
  unique_blocks=scaled['unique_blocks'],object_bytes=scaled['object_bytes'],crop=scaled['crop'],quantized=False,
  background_and_fixed='offline intermediate only',approved_player_override_binding=scaled['approved_player_override_binding'])
 status['object_video']=dict(entry='native_object_begin/native_object_drawing/native_object_finish/native_object_commit',
  frame_storage_max_bytes=4352,limits=dict(oam=128,palettes=16,tiles=1024),reuse='frame-local, no persistent cache',
  source_resolver_connected=False,dynamic_zoom_implemented=False,physical_main_connected=False,hardware_verified=False,
  commit='DMA3 inVBlank/forcedblank, modes0..2, serialized caller; aggregate transfer budget pending')
 status['implementation_only'].extend([
  '0.5.94 full byte-range dynamiczoom via GBA affine plus offline tiny-axis variants;32matrix sharing;64high chunks split to avoid128high wrap',
  'Source028DFE/0294B0 part flip/offset/priority/alternate-mode resolver; explicit normalized anchor/palette and GBA layer input',
  'Palette2C30 target interpolation, source32x32 RGB LUT, period/progress/FFFF completion, globaldarken/lighten/bit3, actor generation lifecycle',
  'Explicit low/high palette banks: session49records and scene/result/script151 callers updated; separate palette service context and world end-of-tick update',
  '227C+3asm167headers compilation/group29765900 bytes; noROM/gameplay/performance execution'])
 status['graphics']['scaled'].update(map_variants=scaled['map_variants'],tiny_axis_factors=scaled['tiny_axis_factors'])
 status['graphics']['remaining']=['Live actor emission/camera/visibility/hit-palette selection; inter-plane pools and budget outputs; water reflection',
  'Approved hand-repaired player pixels binding into all-pose renderer',
  'Full offlineBG/FIX composition and physicalHUD; video jobs/main/audio integration',
  'Final memory/layout/performance verification after full integration']
 status['object_video'].update(dynamic_zoom_implemented=True,source_part_resolver_implemented=True,
  source_resolver_connected=False,affine_limit=32,
  zoom_sampling='GBA affine plus offline extreme-axis nearest; not exact NeoGeo shrink masks')
 status['palettes']=dict(source='generated/render-source.json',entry='native_palette_attach/native_palette_frame/native_palette_actor',
  frame_connected='play_world_tick after entity chain when attached',hardware_main_connected=False,
  interpolation='Source2C30 target blend, not list cycling; extracted32x32 ramp; word countdown andFFFF immediate',
  banks='canonical low14E00 0..1023, high1CE00 1024..1535',
  remaining=['Exact source dynamic48slot reference allocation/failure semantics','Live renderer palette selection and hardwareBG/FIX output'])
 audio=json.loads((g/'native-audio-source.json').read_text())
 status['implementation_only'].extend([
  '0.5.95 all authored13music/150SFX/27voice plus2control sequences ->192mono IMA assets,9435656bytes;11observed loops,299source reply events,offline loop-state integrity only',
  'Music+8SFX IMA mixer,header stop/group/priority rules,music fades,audible-time replies,realDMA1 FIFO/Timer0+1 backend;8192Hzmono adaptation,not YM channel synthesis;external tempo08 explicitly unsupported',
  'Shared BIOS IRQ dispatcher with independent serial7/audio9 ownership and protected registration/stop; actual16bit GBA2P SIO115200/Timer2/packet boundary doublebuffers',
  '128bit source+asset build fingerprint,hostsession+all33RNG words,CRC16,input/frame/ACK barrier,semantic checksum and180VBlank progress timeout; no fabricated remote input',
  'NativeRuntime linked frame adapter rejects direct pump and consumeshost audible reply; actual main/frontend/two-device validation remain',
  'Source496/497 screen anchor conversion,ordinary damage blink and per-part water reflection; synchronous AN_DRAW output sink scoped byNativePlayWorld; concrete collector/manual13drawing sites remain',
  'Paused consumed opportunity preparesFIX/HUD; rawsprite/FIX data split into separate GC sections',
  '234C+4asm172headers ARM compile/group39234688bytes only; no newROM/gameplay/emulator/regression/performance execution'])
 status['audio']=dict(source='generated/native-audio-source.json',integrity='generated/native-audio-integrity.json',
  entry='native_audio_gba_init/native_audio_transport',assets=len(audio['assets']),bytes=audio['bytes'],sample_rate=audio['sample_rate'],
  loops=11,reply_events=299,hardware_backend_implemented=True,physical_main_connected=False,hardware_verified=False,
  limitations=audio['limitations'],remaining=['Physical main/IRQ startup','External tempo07/08','Source channel-stealing adaptation review','Integrated listening/reply/timing/underrun verification'])
 status['link']=dict(entry='native_link_gba_init/native_link_gba_poll/native_runtime_attach_link/native_runtime_link_pump',
  hardware='16bit wired multiplayer115200,serialIRQ7,Timer2',protocol=1,crc='CRC16CCITT',build_fingerprint='128bit from executable sources/headers/incbin assets',
  barrier='Freeze each local input and require peer input plus ACK for same frame',rng='All33source words fromhost beforebegin',
  timeout_vblanks=180,checksum='Semantic partial state, no raw pointers/padding; not allprivate actor fields',
  physical_main_connected=False,hardware_verified=False,
  remaining=['Frontend link selection/wait/leave/lost/return','Actual main polling and handshake VBlank producer','Two GBA fullcampaign/disconnect/audio/IRQ timing validation'])
 status['graphics']['remaining'][0]='Actual owner snapshot collector/manual13drawsites;hit-palette and48slot bindings;source pool/BG priorities,budget inputs;apply reflection gates tolive owners'
 status['object_video'].update(source_anchor_helpers=True,source_blink_helper=True,per_part_reflection=True,
  animation_draw_hook=True,actual_world_collector=False)
 status['runtime'].update(audio_hardware_backend_implemented=True,link_hardware_backend_implemented=True,
  shared_irq_backend_implemented=True,paused_prepare=True,threading='Runtime serialized main caller; audio/link hardware have protected shared IRQ and queues')
 status['runtime']['remaining']=['Concrete full video backend and hardware main/IRQ startup','Audio external tempo/source-channel adaptation and integrated verification',
  'Title/attract/tutorial/ranking/return and link frontend','106F4B consumer','59D1/120D5/318A3A4','Two-device integration','Integrated tests after full implementation']
 status['session']['remaining']=status['runtime']['remaining']+['Special/debug scene selection']
 status['not_connected']=[s.replace('physical scene/audio backends','full physical video and main/audio hookup')for s in status['not_connected']]
 status['pending']+=['Native audio and physical link main/frontend integration; full video/HUD and all three exceptions remain; see remaining-work-0.5.95.ko.md']
 status['implementation_only'].extend([
  '0.5.96 NativeObjectScene captures actual owner pose,zoom,part reflection,program/drawing and palette state at AN_DRAW; World begin/end hooks,13manual emission sites,prepare/OAM/DMA3 component',
  'Common fixed hit palette bindings; source138FE is1C+74,not timer; corrected gun0x76,Slugoverlay8F90,wheelFFFF fixedFF,result orbit32 horizontalzoom',
  'Palette serial+entity generation protects queued images from deletion/rebinding/recycle; retained native palette state update is explicitly NOT source48-slot reuse semantics',
  '210source sort keys and380strip records distinguished; native210map submission cap is conservative.5E804 reads PREVIOUS VBlank free strips; producer remains unconnected',
  '235C+4asm173headers compile/group39240704bytes;World65288,Pool63504,ObjectScene26964,PaletteWorld26636,SceneRuntime4532ARM bytes; noROM or game tests'])
 status['object_video'].update(entry='native_object_scene_attach/native_object_scene_prepare/native_object_scene_commit',
  actual_world_collector=True,source_resolver_connected=True,manual_draw_sites=13,
  capture_storage_bytes=26964,submitted_map_limit=210,source_strip_records=380,
  lifetime='Capture at source emission; no live entity pointer retained after callback',
  layer_mapping='Required caller per-part source-priority/forced-pool resolver; whole BG integration pending',
  sprite_budget_producer_connected=False)
 status['palettes'].update(capture_entry='native_palette_capture/native_palette_resolve_capture',
  storage_bytes=26636,hit_palette_selection=True,fixed_wheel_ff=True,
  lifetime='Serial+generation; post-frame live binding or retained native state. Not exact source release/reuse timing',
  remaining=['Exact source48-slot allocation/sharing/history/release/failure semantics','Physical BG/FIX palette output and complete main hookup'])
 status['graphics']['remaining'][0]='Exact48palette allocation/reuse; source clipped strip occupancy and scene pool/BG priority producer; physical full video/main hookup'
 status['pending']=[
  'FullBG/FIX/HUD offline composition and physical output; approved player repair binding across all poses',
  'Exact48-slot palette allocator and prior-frame free-strip producer; per-scene sprite pools/BG priorities',
  'Whole renderer/audio/sharedIRQ/NativeRuntime/2P hardware main connection and final cartridge/RAM layout',
  'Title/attract/tutorial/external ranking/return and 2P selection/wait/leave/lost frontend',
  'Audio07/08 tempo and premixed source channel-stealing adaptation review',
  '59D1/120D5/318A3A4 unresolved;106F4B consumer and remaining alternate-source comparisons',
  'Integrated1P/2P fullcampaign/results/endings/audio/performance and realtwo-device validation after full source connection']
 status['not_connected']=status['pending'].copy()
 status['implementation_only'].extend([
  '0.5.97 NativeTileVideo attaches actual World column/tile/tile-only-clear/FIX writes: four32x32planes,380physical source columns,40x32FIX and dirty state; preserves attributes on clear and X on hide',
  'All53scene images+659patches (651SceneTilePatch+8M2) offline4x4 BG fragments:35563source tiles/189648unique blocks; all4096FIX glyphs offline2x2 fragments;3133324binary bytes; no resampling in runtime',
  'NativeFixedFrame packs original6x6 spacing into deduplicated8bpp8x8 BG0 tiles, exact shared255opaque-color RGB555 allocator, realDMA3 map/tiles/palette commit with explicitmode/blanking/region gates',
  'Scene1390E low/ordinary/upper/forced pool bounds retained from all16descriptors/43planes; OBJ layer resolver supplies1forced/2ordinary forFIX0/upperBG1/lowerBG3; background within-pool composition stillpending',
  'Native bottom credits/LEVEL move source29->28 (y152..157) to remain visible; ordinaryFIX/sourceworld retain75percent crop4top+4bottom; pause28 unchanged',
  '240C+5asm178headers ARM-Werror compile/group only; no ROM,gameplay,emulator,regression or performance tests'])
 status['graphics']['pixel_conversion']='AllOBJ maps offline75percent; complete typed scene/patch subset indexed4x4 BG fragments;4096FIX2x2 fragments'
 status['graphics']['scaled']['background_and_fixed']='BG fragment assets+source write shadow; FIX physicalBG0 component; fullBG compositor/main remain'
 status['graphics']['remaining']=[
  'Exact48palette allocation/reuse and previous-commit source clipped-strip free count',
  'Approved hand-repaired player pixels binding into all-pose renderer',
  'Within-pool BG composition, auto-animation clock producer, sharedVRAM/palette/DMA budget and physicalmain',
  'Final memory/layout/performance verification after full integration']
 status['object_video']['layer_mapping']='native_tile_video_object_layer uses scene pools:forced1/ordinary2; FIX0/upperBG1/lowerBG3. Whole BG composition stillpending'
 status['palettes']['remaining']=['Exact source48-slot allocation/sharing/history/release/failure semantics','Shared physicalBG/FIX budget and complete main hookup']
 status['tile_video']=dict(source='generated/tile-video-source.json',connection='generated/tile-video-connection.json',
  world_callbacks=True,source_column_count=380,scene_images=53,patches=659,bg_tiles=35563,bg_blocks=189648,fix_tiles=4096,binary_bytes=3133324,
  world_state_bytes=21912,fixed_frame_bytes=42572,shared_palette_bytes=1540,
  background_compositor=False,fix_gba_component=True,main_connected=False,source_free_strip_producer=False,
  exact_color_allocator=True,quantized=False,bottom_hud_row=28,
  limitations=['Four source BG layers plusFIX exceed a direct four-GBA-BG mapping','No integrated palette/VRAM/DMA/RAM/performance or gameplay verification'])
 status['pending']=[
  'Source-background within-pool composition, auto-animation clock, sharedVRAM/BGpalette/DMA plan, physical video/main; approved player repairs',
  'Exact48-slot palette allocator and previous-commit source free-strip producer',
  'Whole renderer/audio/sharedIRQ/NativeRuntime/2P hardware main connection and final cartridge/RAM layout',
  'Title/attract/tutorial/external ranking/return and2P selection/wait/leave/lost frontend',
  'Audio07/08 tempo and premixed source channel-stealing adaptation review',
  '59D1/120D5/318A3A4 unresolved;106F4B consumer and remaining alternate-source comparisons',
  'Integrated1P/2P fullcampaign/results/endings/audio/performance and realtwo-device validation after full source connection']
 status['not_connected']=status['pending'].copy()
 apply_current_integration(status,g,version)
 (g/'port-status.json').write_text(json.dumps(status,indent=2)+'\n')
 manifest=dict(version=version,complete=False,kind='source implementation checkpoint; partial ROM execution evidence, not complete release',
  commit=status['commit'],prom_sha256=json.loads((g/'player-source-catalog.json').read_text())['prom_sha256'],
  files={str(p.relative_to(R)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(
   [R/'VERSION',R/'README.ko.md',R/'AGENTS.md']+[p for folder in ['src','tools','generated','docs','tests'] for p in (R/folder).rglob('*')])
   if p.is_file() and '__pycache__' not in p.parts and p.name!='provenance.json'})
 (g/'provenance.json').write_text(json.dumps(manifest,indent=2)+'\n')
 out=R.parent/'release';out.mkdir(exist_ok=True);target=out/f'metal-slug-gba-native-v{version}-source-wip.zip'
 with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
  for p in sorted(R.rglob('*')):
   if not p.is_file():continue
   rel=p.relative_to(R)
   if rel.parts[0] in ('build','dist') or '__pycache__' in rel.parts or any(v.startswith('.') for v in rel.parts):continue
   z.write(p,Path('metal-slug-native')/rel)
 print(json.dumps(dict(source=str(target),bytes=target.stat().st_size,sha256=hashlib.sha256(target.read_bytes()).hexdigest(),complete=False)))
if __name__=='__main__':main()
