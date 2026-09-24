DELETE FROM `command` WHERE `name` = 'trashtocash';
INSERT INTO `command` (`name`, `security`, `help`) VALUES
('trashtocash', 0, 'Syntax: .trashtocash [on|off]\n\nTurns automatic selling of looted gray items on or off for your character. With no argument, it flips the current setting.');