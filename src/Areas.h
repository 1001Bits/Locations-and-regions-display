#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

// Static lookup mapping each named in-game location to its wiki-style "area"
// subtitle. Sourced verbatim from
// https://fallout.fandom.com/wiki/Fallout_4_locations (Commonwealth + Glowing
// Sea + Far Harbor + Nuka-World sections). The lookup is case-insensitive on
// the location name and returns the area heading exactly as the wiki tabulates
// it. Entries that aren't in the table fall through to the parent-chain
// fallback in RE.h.
namespace Areas
{
	namespace detail
	{
		[[nodiscard]] inline std::string ToLower(std::string_view a_str)
		{
			std::string out(a_str);
			for (auto& c : out) {
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			}
			return out;
		}

		struct Entry
		{
			const char* location;
			const char* area;
		};

		// One entry per (location, area) pair. Areas with a "self" entry
		// (location name == area name) are intentionally omitted so we don't
		// render redundant subtitles like "Diamond City\nDiamond City".
		inline constexpr Entry kEntries[] = {
			// Back Bay
			{ "Boston Public Library", "Back Bay" },
			{ "Copley station", "Back Bay" },
			{ "Dartmouth Professional Building", "Back Bay" },
			{ "Hubris Comics", "Back Bay" },
			{ "Jimbo's Tower", "Back Bay" },
			{ "Layton Towers", "Back Bay" },
			{ "Prost Bar", "Back Bay" },
			{ "Shenley's Oyster Bar", "Back Bay" },
			{ "Trinity Church", "Back Bay" },
			{ "Trinity Plaza", "Back Bay" },
			{ "Trinity Tower", "Back Bay" },
			{ "Vault 114", "Back Bay" },
			{ "Warren Theater", "Back Bay" },
			{ "Wilson Atomatoys corporate HQ", "Back Bay" },

			// Beacon Hill
			{ "Beacon Hill apartments", "Beacon Hill" },
			{ "Boston Bugle building", "Beacon Hill" },
			{ "Cabot House", "Beacon Hill" },
			{ "Lil' Gentry playground", "Beacon Hill" },
			{ "Vault-Tec Regional HQ", "Beacon Hill" },

			// Boston Common
			{ "Boylston Club", "Boston Common" },
			{ "Massachusetts State House", "Boston Common" },
			{ "Old Granary Burying Ground", "Boston Common" },
			{ "Park Street station", "Boston Common" },
			{ "Swan's Pond", "Boston Common" },

			// Boston Harbor
			{ "Columbus Park", "Boston Harbor" },
			{ "Custom House Tower", "Boston Harbor" },
			{ "Harbormaster Hotel", "Boston Harbor" },
			{ "Long Wharf", "Boston Harbor" },
			{ "The Shamrock Taphouse", "Boston Harbor" },
			{ "Yangtze", "Boston Harbor" },

			// Cambridge
			{ "C.I.T. ruins", "Cambridge" },
			{ "Cambridge Academic Center", "Cambridge" },
			{ "Cambridge campus diner", "Cambridge" },
			{ "Cambridge church", "Cambridge" },
			{ "Cambridge construction site", "Cambridge" },
			{ "Cambridge crater", "Cambridge" },
			{ "Cambridge hardware store", "Cambridge" },
			{ "Cambridge Police Station", "Cambridge" },
			{ "Cambridge Polymer Labs", "Cambridge" },
			{ "Cambridge raider base", "Cambridge" },
			{ "Camp Kendall", "Cambridge" },
			{ "Campus law offices", "Cambridge" },
			{ "College Square", "Cambridge" },
			{ "College Square station", "Cambridge" },
			{ "Collegiate administration building", "Cambridge" },
			{ "East C.I.T. raider camp", "Cambridge" },
			{ "Fraternal Post 115", "Cambridge" },
			{ "Greenetech Genetics", "Cambridge" },
			{ "Ivy League Bridge", "Cambridge" },
			{ "Kendall Hospital", "Cambridge" },
			{ "Kendall Parking", "Cambridge" },
			{ "Longfellow Bridge", "Cambridge" },
			{ "Mass Chemical", "Cambridge" },
			{ "Monsignor Plaza", "Cambridge" },
			{ "Plumber's Secret", "Cambridge" },
			{ "Prospect Hill", "Cambridge" },
			{ "Science Center gift shop", "Cambridge" },
			{ "Ticonderoga", "Cambridge" },
			{ "Union's Hope Cathedral", "Cambridge" },

			// Charlestown
			{ "BADTFL regional office", "Charlestown" },
			{ "Bunker Hill", "Charlestown" },
			{ "Charlestown Laundry", "Charlestown" },
			{ "Drug den", "Charlestown" },
			{ "Weatherby Savings & Loan", "Charlestown" },

			// Diamond City
			{ "Abbot's house", "Diamond City" },
			{ "All Faiths Chapel", "Diamond City" },
			{ "Chem-I-Care", "Diamond City" },
			{ "Choice Chops", "Diamond City" },
			{ "Codman residence", "Diamond City" },
			{ "Colonial Taphouse", "Diamond City" },
			{ "Commonwealth Weaponry", "Diamond City" },
			{ "Cooke residence", "Diamond City" },
			{ "Diamond City market", "Diamond City" },
			{ "Diamond City Radio", "Diamond City" },
			{ "Diamond City Surplus", "Diamond City" },
			{ "Doc Crocker's house", "Diamond City" },
			{ "Doctor Sun's house", "Diamond City" },
			{ "Dugout Inn", "Diamond City" },
			{ "Earl Sterling's house", "Diamond City" },
			{ "Fallon's Basement", "Diamond City" },
			{ "Greenhouse", "Diamond City" },
			{ "Hawthorne residence", "Diamond City" },
			{ "Home Plate", "Diamond City" },
			{ "Kathy & John's Super Salon", "Diamond City" },
			{ "Kellogg's house", "Diamond City" },
			{ "Latimer residence", "Diamond City" },
			{ "Mayor McDonough's office", "Diamond City" },
			{ "Mega Surgery Center", "Diamond City" },
			{ "Pembroke residence", "Diamond City" },
			{ "Power Noodles", "Diamond City" },
			{ "Publick Occurrences", "Diamond City" },
			{ "Schoolhouse", "Diamond City" },
			{ "Science Center", "Diamond City" },
			{ "Security office", "Diamond City" },
			{ "Sheng Kawolski's house", "Diamond City" },
			{ "Swatters", "Diamond City" },
			{ "The Wall", "Diamond City" },
			{ "Valentine Detective Agency", "Diamond City" },

			// East Boston
			{ "Boston Airport", "East Boston" },
			{ "Boston Airport ruins", "East Boston" },
			{ "Drumlin Diner", "East Boston" },  // there are 3 Drumlin Diners — wiki disambiguates; in-game prob shares name
			{ "East Boston police station", "East Boston" },
			{ "East Boston Preparatory School", "East Boston" },
			{ "Easy City Downs", "East Boston" },
			{ "RobCo Sales & Service Center", "East Boston" },
			{ "The Mechanist's lair", "East Boston" },
			{ "The Prydwen", "East Boston" },

			// Esplanade
			{ "Barricade and rooftops", "Esplanade" },
			{ "Charles View Amphitheater", "Esplanade" },
			{ "Commonwealth Avenue", "Esplanade" },
			{ "Footbridge", "Esplanade" },
			{ "HalluciGen, Inc.", "Esplanade" },
			{ "Holy Mission Congregation", "Esplanade" },
			{ "Marlborough house", "Esplanade" },

			// Financial District
			{ "35 Court", "Financial District" },
			{ "Baxter building", "Financial District" },
			{ "Broken monorail", "Financial District" },
			{ "Commonwealth Bank", "Financial District" },
			{ "Congress Street garage", "Financial District" },
			{ "Fallen skybridge", "Financial District" },
			{ "Faneuil Hall", "Financial District" },
			{ "Garden terrace", "Financial District" },
			{ "Haymarket Mall", "Financial District" },
			{ "Joe's Spuckies", "Financial District" },
			{ "Mass Fusion building", "Financial District" },
			{ "Mass Fusion executive suite", "Financial District" },
			{ "Old Corner Bookstore", "Financial District" },
			{ "Pinnacle Highrise", "Financial District" },
			{ "Postal Square", "Financial District" },
			{ "Water Street apartments", "Financial District" },

			// Fort Hagen
			{ "Fort Hagen Command Center", "Fort Hagen" },
			{ "Fort Hagen filling station", "Fort Hagen" },
			{ "Fort Hagen hangar", "Fort Hagen" },
			{ "Fort Hagen satellite array", "Fort Hagen" },
			{ "Greater Mass Blood Clinic", "Fort Hagen" },

			// General Atomics Galleria
			{ "Back Alley Bowling", "General Atomics Galleria" },
			{ "General Atomics outlet", "General Atomics Galleria" },
			{ "Handy Eats", "General Atomics Galleria" },
			{ "Madden's Gym", "General Atomics Galleria" },
			{ "Pinelli's Bakery", "General Atomics Galleria" },

			// The Glowing Sea
			{ "Atlantic Offices", "The Glowing Sea" },
			{ "Capsized factory", "The Glowing Sea" },
			{ "Cave", "The Glowing Sea" },
			{ "Crater of Atom", "The Glowing Sea" },
			{ "Decayed reactor site", "The Glowing Sea" },
			{ "Decrepit factory", "The Glowing Sea" },
			{ "Edge of the Glowing Sea", "The Glowing Sea" },
			{ "Federal supply cache 84NE", "The Glowing Sea" },
			{ "Federal Surveillance Center K-21B", "The Glowing Sea" },
			{ "Forgotten church", "The Glowing Sea" },
			{ "Hopesmarch Pentecostal Church", "The Glowing Sea" },
			{ "O'Neill Family Manufacturing", "The Glowing Sea" },
			{ "Parking garage", "The Glowing Sea" },
			{ "Relay tower 0DB-521", "The Glowing Sea" },
			{ "Rocky Cave", "The Glowing Sea" },
			{ "Sentinel site", "The Glowing Sea" },
			{ "Skylanes Flight 1665", "The Glowing Sea" },
			{ "Vertibird wreckage", "The Glowing Sea" },

			// Malden
			{ "Garage alcove", "Malden" },
			{ "Greentop Nursery", "Malden" },
			{ "Malden Center", "Malden" },
			{ "Malden drainage", "Malden" },
			{ "Malden Middle School", "Malden" },
			{ "Malden police station", "Malden" },
			{ "Med-Tek Research", "Malden" },
			{ "Medford Memorial Hospital", "Malden" },
			{ "Old Gullet sinkhole", "Malden" },
			{ "Ruined clinic", "Malden" },
			{ "Slocum's Joe Corporate HQ", "Malden" },
			{ "Vault 75", "Malden" },

			// Nahant
			{ "Croup Manor", "Nahant" },
			{ "Nahant Bar and Restaurant", "Nahant" },
			{ "Nahant boathouse", "Nahant" },
			{ "Nahant Chapel", "Nahant" },
			{ "Nahant junkyard", "Nahant" },
			{ "Nahant Oceanological Society", "Nahant" },
			{ "Nahant Sherrif's Department", "Nahant" },
			{ "Nahant Wharf", "Nahant" },

			// Natick
			{ "Hillside home", "Natick" },
			{ "Natick Banks", "Natick" },
			{ "Natick Police Department", "Natick" },
			{ "Natick power station", "Natick" },
			{ "Poseidon reservoir", "Natick" },
			{ "Roadside Pines Motel", "Natick" },
			{ "Settler campsite", "Natick" },

			// North End
			{ "Boxing gym", "North End" },
			{ "Hoarder's apartment", "North End" },
			{ "Mean Pastries", "North End" },
			{ "North End graveyard", "North End" },
			{ "Old North Church", "North End" },
			{ "Paul Revere House", "North End" },
			{ "Paul Revere Monument", "North End" },
			{ "Pickman Gallery", "North End" },
			{ "Pizza parlor", "North End" },
			{ "Railroad HQ", "North End" },
			{ "Valenti station", "North End" },
			{ "Wharfside cottage", "North End" },

			// Quincy ruins
			{ "Fentons Food Stuffs", "Quincy ruins" },
			{ "Guns Guns Guns", "Quincy ruins" },
			{ "Peabody house", "Quincy ruins" },
			{ "Quincy apartments", "Quincy ruins" },
			{ "Quincy church", "Quincy ruins" },
			{ "Quincy diner", "Quincy ruins" },
			{ "Quincy pharmacy", "Quincy ruins" },
			{ "Quincy police station", "Quincy ruins" },
			{ "Quincy Quarries", "Quincy ruins" },
			{ "The Hole in the Wall", "Quincy ruins" },
			{ "Vault 88", "Quincy ruins" },

			// Revere
			{ "Gibson Point Pier", "Revere" },
			{ "Reeb Marina", "Revere" },
			{ "Revere Beach station", "Revere" },
			{ "Revere satellite array", "Revere" },

			// Salem
			{ "Museum of Witchcraft", "Salem" },
			{ "Rook family house", "Salem" },
			{ "Sandy Coves Convalescent Home", "Salem" },

			// Sanctuary Hills
			{ "Minutemen Monument", "Sanctuary Hills" },
			{ "Misty Lake", "Sanctuary Hills" },
			{ "Old North Bridge", "Sanctuary Hills" },
			{ "Root cellar", "Sanctuary Hills" },
			{ "Sole Survivor's house", "Sanctuary Hills" },

			// South Boston
			{ "Andrew station", "South Boston" },
			{ "Bus and apartment wreckage", "South Boston" },
			{ "Dorchester Heights Monument", "South Boston" },
			{ "Factory", "South Boston" },
			{ "Four Leaf Fishpacking plant", "South Boston" },
			{ "General Atomics factory", "South Boston" },
			{ "Gwinnett Brewery", "South Boston" },
			{ "Hawthorne Estate", "South Boston" },
			{ "Joe's Spuckies sandwich shop", "South Boston" },
			{ "L Street bathhouse", "South Boston" },
			{ "Marowski's chem lab", "South Boston" },
			{ "Rusting ship and stilt cabin", "South Boston" },
			{ "Sleepwalker's place", "South Boston" },
			{ "South Boston Church", "South Boston" },
			{ "South Boston High School", "South Boston" },
			{ "South Boston military checkpoint", "South Boston" },
			{ "South Boston Police Department", "South Boston" },
			{ "Sullivan's", "South Boston" },
			{ "The Castle", "South Boston" },
			{ "The Gwinnett Restaurant", "South Boston" },
			{ "Union Man's Circle", "South Boston" },

			// The Fens
			{ "Anna's Cafe", "The Fens" },
			{ "Back Street Apparel", "The Fens" },
			{ "Bridgeway Trust", "The Fens" },
			{ "Brookline building", "The Fens" },
			{ "Evans Way Cul-de-Sac", "The Fens" },
			{ "Fens Cafe", "The Fens" },
			{ "Fens Street sewer", "The Fens" },
			{ "Fens Way station", "The Fens" },
			{ "Flagon Tunnel", "The Fens" },
			{ "Founder's Triangle", "The Fens" },
			{ "Hangman's Alley", "The Fens" },
			{ "Hardware Town", "The Fens" },
			{ "Hidden diner", "The Fens" },
			{ "Parkview Apartments", "The Fens" },
			{ "Police Precinct 8", "The Fens" },
			{ "Raider back-alley camp", "The Fens" },
			{ "South Fens Tower", "The Fens" },
			{ "Wreck of the USS Riptide", "The Fens" },

			// The Institute
			{ "FEV lab", "The Institute" },
			{ "Institute Advanced Systems", "The Institute" },
			{ "Institute BioScience", "The Institute" },
			{ "Institute concourse", "The Institute" },
			{ "Institute reactor", "The Institute" },
			{ "Institute Robotics", "The Institute" },
			{ "Institute SRB", "The Institute" },
			{ "Institute sublevel 21-D", "The Institute" },
			{ "Old Robotics", "The Institute" },
			{ "Public works maintenance area", "The Institute" },

			// Theater District
			{ "Back alley camp", "Theater District" },
			{ "Combat Zone", "Theater District" },
			{ "D.B. Technical High School", "Theater District" },
			{ "Freeway pileup", "Theater District" },
			{ "Hester's Consumer Robotics", "Theater District" },
			{ "Hub 360", "Theater District" },
			{ "Mass Bay Medical Center", "Theater District" },
			{ "Medical Center metro", "Theater District" },
			{ "Pearwood Residences", "Theater District" },
			{ "Slocum's Joe", "Theater District" },
			{ "Ticker Tape Lounge", "Theater District" },

			// University Point
			{ "Sedgwick Hall", "University Point" },
			{ "University Credit Union", "University Point" },
			{ "University Point Pharmacy", "University Point" },

			// West Roxbury
			{ "Fallon's department store", "West Roxbury" },
			{ "Milton General Hospital", "West Roxbury" },
			{ "Milton parking garage", "West Roxbury" },
			{ "Shaw High School", "West Roxbury" },
			{ "West Roxbury station", "West Roxbury" },

			// Far Harbor
			{ "Acadia", "Far Harbor" },
			{ "Acadia National Park", "Far Harbor" },
			{ "National Park campground", "Far Harbor" },
			{ "National Park HQ", "Far Harbor" },
			{ "National Park visitor's center", "Far Harbor" },
			{ "Aldersea Day Spa", "Far Harbor" },
			{ "Atom's Spring", "Far Harbor" },
			{ "Bar Harbor Museum", "Far Harbor" },
			{ "Basement armory", "Far Harbor" },
			{ "Beaver Creek Lanes", "Far Harbor" },
			{ "Briney's Bait and Tackle", "Far Harbor" },
			{ "Brooke's Head Lighthouse", "Far Harbor" },
			{ "Children of Atom shrine", "Far Harbor" },
			{ "Cliff's Edge Hotel", "Far Harbor" },
			{ "Vault 118", "Far Harbor" },
			{ "Cranberry Island", "Far Harbor" },
			{ "Cranberry Island Bog", "Far Harbor" },
			{ "Cranberry Island Docks", "Far Harbor" },
			{ "Cranberry Island supply shed", "Far Harbor" },
			{ "Dalton farm", "Far Harbor" },
			{ "DiMA's cache", "Far Harbor" },
			{ "Eagle's Cove Tannery", "Far Harbor" },
			{ "Echo Lake Lumber", "Far Harbor" },
			{ "Eden Meadows Cinemas", "Far Harbor" },
			{ "Emmet's Causeway", "Far Harbor" },
			{ "The Last Plank", "Far Harbor" },
			{ "Fringe Cove docks", "Far Harbor" },
			{ "Glowing Grove", "Far Harbor" },
			{ "Haddock Cove", "Far Harbor" },
			{ "Harbor Grand Hotel", "Far Harbor" },
			{ "Horizon Flight 1207", "Far Harbor" },
			{ "Huntress Island", "Far Harbor" },
			{ "Kawaketak Station", "Far Harbor" },
			{ "Kitteredge Pass", "Far Harbor" },
			{ "Longfellow's cabin", "Far Harbor" },
			{ "MS Azalea", "Far Harbor" },
			{ "Nakano residence", "Far Harbor" },
			{ "Northwood Ridge Quarry", "Far Harbor" },
			{ "Oceanarium", "Far Harbor" },
			{ "Old pond house", "Far Harbor" },
			{ "Pine Crest Cavern", "Far Harbor" },
			{ "Pump control", "Far Harbor" },
			{ "Radiant crest shrine", "Far Harbor" },
			{ "Rayburn Point", "Far Harbor" },
			{ "Red Death Island", "Far Harbor" },
			{ "Rock Point camp", "Far Harbor" },
			{ "Rope bridge complex", "Far Harbor" },
			{ "Ruined church", "Far Harbor" },
			{ "Ruined radio tower", "Far Harbor" },
			{ "Southwest Harbor", "Far Harbor" },
			{ "The Nucleus", "Far Harbor" },
			{ "Nucleus Command Center", "Far Harbor" },
			{ "The Vessel", "Far Harbor" },
			{ "Vim! Pop factory", "Far Harbor" },
			{ "Waves Crest Orphanage", "Far Harbor" },
			{ "Wind farm maintenance", "Far Harbor" },
			{ "Zephyr Ridge camp", "Far Harbor" },

			// Nuka-World
			{ "Nuka-World transit center", "Nuka-World" },
			{ "Nuka-World exterior northeast", "Nuka-World" },
			{ "Bradberton", "Nuka-World" },
			{ "Bradberton overpass", "Nuka-World" },
			{ "Dead end houses", "Nuka-World" },
			{ "Distiller shack", "Nuka-World" },
			{ "Little Nuka Gift Shop", "Nuka-World" },
			{ "Morton residence", "Nuka-World" },
			{ "Northpoint reservoir", "Nuka-World" },
			{ "Nuka-World Red Rocket", "Nuka-World" },
			{ "Pool house", "Nuka-World" },
			{ "Reservoir houses", "Nuka-World" },
			{ "Nuka-World exterior northwest", "Nuka-World" },
			{ "Cappy and Bottle shrine", "Nuka-World" },
			{ "Nuka-World power plant", "Nuka-World" },
			{ "Northwest maintenance buildings", "Nuka-World" },
			{ "Picnic and campsite", "Nuka-World" },
			{ "Red tent camp", "Nuka-World" },
			{ "TV tree", "Nuka-World" },
			{ "Wind power camp", "Nuka-World" },
			{ "Nuka-World exterior southeast", "Nuka-World" },
			{ "Evan's home", "Nuka-World" },
			{ "Monorail tunnel", "Nuka-World" },
			{ "Nuka-Express", "Nuka-World" },
			{ "Nuka-station", "Nuka-World" },
			{ "Nuka-World access tunnels", "Nuka-World" },
			{ "Placard building", "Nuka-World" },
			{ "Southeast town", "Nuka-World" },
			{ "Wixon's Shovel Museum", "Nuka-World" },
			{ "Nuka-World exterior southwest", "Nuka-World" },
			{ "Abandoned farmland", "Nuka-World" },
			{ "Dunmore homestead", "Nuka-World" },
			{ "Grandchester Mystery Mansion", "Nuka-World" },
			{ "Hangar camp", "Nuka-World" },
			{ "Hubologist's camp", "Nuka-World" },
			{ "Nuka-World junkyard", "Nuka-World" },
			{ "Stingwing nests", "Nuka-World" },
			{ "Super mutant shack", "Nuka-World" },
			{ "Toxic dump", "Nuka-World" },
			{ "Toxic lake", "Nuka-World" },
			{ "World's Largest Fire Hydrant", "Nuka-World" },
			{ "Dry Rock Gulch", "Nuka-World" },
			{ "Doc Phosphate's Saloon", "Nuka-World" },
			{ "Dry Rock Gulch employee area", "Nuka-World" },
			{ "Dry Rock Gulch theater", "Nuka-World" },
			{ "Mad Mulligan's Minecart Coaster", "Nuka-World" },
			{ "Main Street", "Nuka-World" },
			{ "Ol' Sugartop", "Nuka-World" },
			{ "Galactic Zone", "Nuka-World" },
			{ "ArcJet G-Force", "Nuka-World" },
			{ "Blast Off!", "Nuka-World" },
			{ "Handy Whirl", "Nuka-World" },
			{ "Nuka-Galaxy", "Nuka-World" },
			{ "Nuka Rockets", "Nuka-World" },
			{ "RobCo Battlezone", "Nuka-World" },
			{ "Spacewalk", "Nuka-World" },
			{ "Splashdown", "Nuka-World" },
			{ "Star Market", "Nuka-World" },
			{ "Starlight Interstellar Theater", "Nuka-World" },
			{ "Starport Nuka", "Nuka-World" },
			{ "Vault-Tec: Among the Stars", "Nuka-World" },
			{ "Kiddie Kingdom", "Nuka-World" },
			{ "Candy Town Playground", "Nuka-World" },
			{ "Carousel", "Nuka-World" },
			{ "Employee tunnels", "Nuka-World" },
			{ "Ferris Wheel", "Nuka-World" },
			{ "Fun House", "Nuka-World" },
			{ "King Cola's Castle", "Nuka-World" },
			{ "King Cola's Castle Tower", "Nuka-World" },
			{ "King Cola's Court", "Nuka-World" },
			{ "Nuka-Racers", "Nuka-World" },
			{ "Rally Rollers", "Nuka-World" },
			{ "Teacups", "Nuka-World" },
			{ "Nuka-Cola bottling plant", "Nuka-World" },
			{ "Secure beverageer lab", "Nuka-World" },
			{ "World of Refreshment", "Nuka-World" },
			{ "Nuka Island", "Nuka-World" },
			{ "Nuka-Town USA", "Nuka-World" },
			{ "Bradberton Amphitheater", "Nuka-World" },
			{ "Bradberton's office", "Nuka-World" },
			{ "Cappy's Cafe", "Nuka-World" },
			{ "Cola-cars arena", "Nuka-World" },
			{ "Fizztop Grille", "Nuka-World" },
			{ "Fizztop Mountain", "Nuka-World" },
			{ "Nuka-Cade", "Nuka-World" },
			{ "Nuka-Town backstage", "Nuka-World" },
			{ "Nuka-Town market", "Nuka-World" },
			{ "Nuka-World maintenance shed", "Nuka-World" },
			{ "The Parlor", "Nuka-World" },
			{ "Safari Adventure", "Nuka-World" },
			{ "Angry Anaconda", "Nuka-World" },
			{ "Bear cave", "Nuka-World" },
			{ "Cappy's treehouse", "Nuka-World" },
			{ "Jungle Journey Theater", "Nuka-World" },
			{ "Safari Adventure primate house", "Nuka-World" },
			{ "Safari Adventure reptile house", "Nuka-World" },
			{ "Welcome Center", "Nuka-World" },
		};

		[[nodiscard]] inline const std::unordered_map<std::string, const char*>& GetTable()
		{
			static const auto table = []() {
				std::unordered_map<std::string, const char*> t;
				t.reserve(sizeof(kEntries) / sizeof(kEntries[0]));
				for (const auto& e : kEntries) {
					t.emplace(ToLower(e.location), e.area);
				}
				return t;
			}();
			return table;
		}

		// Top-level area / worldspace names that should NOT receive the
		// "Commonwealth" fallback subtitle. These are the area headings the
		// wiki uses, plus a couple of common worldspace title strings the
		// engine fires when crossing between DLC worldspaces. We'd otherwise
		// render e.g. "Far Harbor\nCommonwealth" which is wrong.
		inline constexpr const char* kTopLevelAreas[] = {
			"Commonwealth",
			"Far Harbor",
			"Nuka-World",
			"Nuka-Town USA",
			"The Glowing Sea",
			"Diamond City",
			"Goodneighbor",
			"Bunker Hill",
			"Sanctuary Hills",
			"Vault 111",
			"The Institute",
		};

		[[nodiscard]] inline const std::unordered_set<std::string>& GetTopLevelSet()
		{
			static const auto set = []() {
				std::unordered_set<std::string> s;
				for (const auto* name : kTopLevelAreas) {
					s.emplace(ToLower(name));
				}
				return s;
			}();
			return set;
		}
	}

	[[nodiscard]] inline const char* GetAreaForLocation(const char* a_locationName)
	{
		if (!a_locationName || *a_locationName == '\0') {
			return nullptr;
		}
		const auto& table = detail::GetTable();
		const auto  it = table.find(detail::ToLower(a_locationName));
		return it != table.end() ? it->second : nullptr;
	}

	// True when the supplied name is itself a top-level area / worldspace
	// title (Far Harbor, Nuka-World, Diamond City, …). Callers use this to
	// suppress the "Commonwealth" fallback subtitle on those entries.
	[[nodiscard]] inline bool IsTopLevelArea(const char* a_name)
	{
		if (!a_name || *a_name == '\0') {
			return false;
		}
		const auto& set = detail::GetTopLevelSet();
		return set.find(detail::ToLower(a_name)) != set.end();
	}
}
