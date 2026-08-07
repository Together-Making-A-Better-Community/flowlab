--
-- PostgreSQL database dump
--

\restrict P7qVC00wFgEpJKcHbkM2XCHMBK2UZfIAnoN97BmZQIPZczBLtEsLolyJxvXznJ1

-- Dumped from database version 15.18 (Debian 15.18-0+deb12u1)
-- Dumped by pg_dump version 15.18 (Debian 15.18-0+deb12u1)

SET statement_timeout = 0;
SET lock_timeout = 0;
SET idle_in_transaction_session_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = on;
SELECT pg_catalog.set_config('search_path', '', false);
SET check_function_bodies = false;
SET xmloption = content;
SET client_min_messages = warning;
SET row_security = off;

SET default_tablespace = '';

SET default_table_access_method = heap;

--
-- Name: readings; Type: TABLE; Schema: public; Owner: seedling
--

CREATE TABLE public.readings (
    id integer NOT NULL,
    received_at timestamp with time zone DEFAULT now(),
    device_id text,
    air_temp_c real,
    humidity real,
    soil_temp_c real,
    soil_wet smallint,
    soil_raw integer,
    soil_percent integer,
    trigger_type text
);


ALTER TABLE public.readings OWNER TO seedling;

--
-- Name: readings_id_seq; Type: SEQUENCE; Schema: public; Owner: seedling
--

CREATE SEQUENCE public.readings_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.readings_id_seq OWNER TO seedling;

--
-- Name: readings_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: seedling
--

ALTER SEQUENCE public.readings_id_seq OWNED BY public.readings.id;


--
-- Name: readings id; Type: DEFAULT; Schema: public; Owner: seedling
--

ALTER TABLE ONLY public.readings ALTER COLUMN id SET DEFAULT nextval('public.readings_id_seq'::regclass);


--
-- Name: readings readings_pkey; Type: CONSTRAINT; Schema: public; Owner: seedling
--

ALTER TABLE ONLY public.readings
    ADD CONSTRAINT readings_pkey PRIMARY KEY (id);


--
-- PostgreSQL database dump complete
--

\unrestrict P7qVC00wFgEpJKcHbkM2XCHMBK2UZfIAnoN97BmZQIPZczBLtEsLolyJxvXznJ1

