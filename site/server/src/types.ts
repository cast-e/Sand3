export type ItemType = 'set' | 'save' | 'stamp' | 'theme';

export type UserRole = 'admin' | 'moderator' | 'user';

export interface User {
  id: string;
  username: string;
  role?: UserRole;
  is_admin?: boolean;
  created_at: string;
}

export interface WorkshopItem {
  id: string;
  user_id?: string | null;
  type: ItemType;
  title: string;
  description: string;
  author: string;
  parent_set_id: string | null;
  version: number;
  set_hash?: string;
  file_path: string;
  file_size: number;
  thumbnail_path?: string;
  meta_json: string;
  likes_count: number;
  favorites_count: number;
  downloads_count: number;
  reports_count: number;
  is_hidden: number;
  created_at: string;
  updated_at: string;
  is_liked?: boolean;
  is_favorited?: boolean;
  parent_set_title?: string;
  forked_from_id?: string | null;
  forked_from_version?: number | null;
  forked_from_title?: string;
  forked_from_author?: string;
  forks_count?: number;
  child_saves_count?: number;
  child_stamps_count?: number;
}

export interface Interaction {
  id: number;
  item_id: string;
  client_uuid: string;
  interaction_type: 'like' | 'favorite';
  created_at: string;
}

export interface Report {
  id: number;
  item_id: string;
  client_uuid: string;
  reason: 'broken' | 'offensive' | 'spam' | 'other';
  details: string;
  created_at: string;
}

